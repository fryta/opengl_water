#include "water_surface.h"
#include <cstdio>
#include <algorithm>
#include <cmath>
#define M_PI 3.14159265358979323846


WaterSurface::WaterSurface(
		float dim_x, float dim_z, int grid_x, int grid_z, 
		float wave_speed, float dt, float damp_factor, uint64 usec_step_time) 
{
	m_dim_x = dim_x;
	m_dim_y = dim_z;
	m_grid_x = grid_x;
	m_grid_y = grid_z;
	m_wave_speed = wave_speed;
	m_dt = dt;
	m_damp_factor = damp_factor;
	m_step = usec_step_time;

	m_u = nullptr;
	m_u_new = nullptr;
	m_v = nullptr;
	m_bar = nullptr;
	m_model_mat = nullptr;
}

WaterSurface::~WaterSurface()
{
	if (m_u != nullptr) 
	{
		for (int i = 0; i < m_grid_x + 2; i++)
			delete[] m_u[i];
		delete[] m_u;
	}
	if (m_u_new != nullptr) 
	{
		for (int i = 0; i < m_grid_x + 2; i++)
			delete[] m_u_new[i];
		delete[] m_u_new;
	}
	if (m_v != nullptr) 
	{
		for (int i = 0; i < m_grid_x + 2; i++)
			delete[] m_v[i];
		delete[] m_v;
	}
	if (m_model_mat != nullptr) 
	{
		for (int i = 0; i < m_grid_x + 2; i++)
			delete[] m_model_mat[i];
		delete[] m_model_mat;
	}
	if (m_bar != nullptr) 
		delete m_bar;
}

bool WaterSurface::init() 
{
	if (m_dim_x == 0 || m_dim_y == 0 || m_grid_x == 0 || m_grid_y == 0)
	{
		fprintf(stderr, "Invalid dimensions of water surface.\n");
		return false;
	}

	m_cell_size_x = m_dim_x / m_grid_x;
	m_cell_size_y = m_dim_y / m_grid_y;

	m_simulation_time = 0;
	m_last_call = 0;

	m_u = new double*[m_grid_x + 2]; // + boundary
	m_u_new = new double*[m_grid_x + 2];
	m_v = new double*[m_grid_x + 2]; 
	m_model_mat = new math::Mat4x4f*[m_grid_x + 2];
	
	for (int i = 0; i < m_grid_x + 2; i++)
	{
		m_u[i] = new double[m_grid_y + 2];
		m_u_new[i] = new double[m_grid_y + 2];
		m_v[i] = new double[m_grid_y + 2];
		m_model_mat[i] = new math::Mat4x4f[m_grid_y + 2];
	}

	for (int i = 0; i < m_grid_x + 2; i++)
		for (int j = 0; j < m_grid_y + 2; j++) 
		{
			// init m_u "with some initeresting func"
			// i.e. m_u[i][j] = -sin(10.0f*float(i) / m_grid_x + 10.0f*float(j) / m_grid_y)*0.4;
			// i.e. m_u[i][j] = -sin(10.0f*float(i) / m_grid_x + 0.4f*(10.0f*float(j) / m_grid_y))*0.4;
			m_u[i][j] = 0.0; // or just wait for interaction
			m_v[i][j] = 0.0f;
			m_model_mat[i][j] = math::Mat4x4f(math::Mat4x4f::I);
		}

	m_bar = new Renderable();
	if (!m_bar->load_bar(m_cell_size_x/2.0f, 1.0f, m_cell_size_y/2.0f))
	{
		fprintf(stderr, "Loading planes failed.\n");
		return false;
	}
	if (!m_bar->addTextures("base", L"../data/water_diff.jpg", nullptr, nullptr))
	{
		fprintf(stderr, "Loading textures for planes failed.\n");
		return false;
	}

	return true;
}

void WaterSurface::render(
	glp::Program& render_program, 
	const math::Mat4x4f& inv_view) const
{
	for (int i = 1; i < m_grid_x + 1; i++)
		for (int j = 1; j < m_grid_y + 1; j++) 
		{
			// transpose bars to proper positions
			math::Vec3f tr = math::Vec3f(-0.5f*m_dim_x + (i - 0.5f)*m_cell_size_x, -1.5f + float(m_u[i][j]), -0.5f*m_dim_y + (j - 0.5f)*m_cell_size_y);
			math::set_translation(m_model_mat[i][j], tr);

			render_program.uniform_mat4x4("model", m_model_mat[i][j].m, true);
			render_program.uniform_mat4x4("modelView", (inv_view*m_model_mat[i][j]).m, true);
			m_bar->render(true);
		}
}

void WaterSurface::update_model(uint64 usec_time, bool force_one_step)
{
	if (force_one_step) 
	{
		m_simulation_time = m_step + 1;
	}
	else 
	{
		m_simulation_time += (usec_time - m_last_call);
		m_last_call = usec_time;
	}
	while (m_simulation_time > m_step) {
		m_simulation_time -= m_step;

		double force;
		for (int i = 1; i <= m_grid_x; i++)
			for (int j = 1; j <= m_grid_y; j++) 
			{
				force = 
					pow(m_wave_speed, 2.0) // c^2
					*(m_u[i-1][j] + m_u[i+1][j] + m_u[i][j-1] + m_u[i][j+1] - 4*m_u[i][j])
					/(m_cell_size_x*m_cell_size_y); // h^2
				m_v[i][j] += force * m_dt;
				m_v[i][j] = m_v[i][j] * m_damp_factor;
				m_u_new[i][j] = m_u[i][j] + m_v[i][j] * m_dt;
			}
		// pointers swap: u <-> u_new
		double** tmp = m_u;
		m_u = m_u_new;
		m_u_new = tmp;

		// clamp on edges
		for (int i = 0; i < m_grid_x + 2; i++)
		{
			m_u[i][0] = m_u[i][1];
			m_u[i][m_grid_y + 1] = m_u[i][m_grid_y];
		}

		for (int j = 0; j < m_grid_y + 2; j++) 
		{
			m_u[0][j] = m_u[1][j];
			m_u[m_grid_x + 1][j] = m_u[m_grid_x][j];
		}

	}
}

void WaterSurface::touch(int x, int y, double strength, double distance)
{
	// include boundary (0 and m_grid_x/y + 1)
	int low_x = std::max(0, x - 10);
	int high_x = std::min(m_grid_x + 1, x + 10);
	int low_y = std::max(0, y - 10);
	int high_y = std::min(m_grid_y + 1, y + 10);

	double change_sum = 0.0;
	for (int i = low_x; i < high_x; i++)
		for (int j = low_y; j < high_y; j++)
		{
			double x_dist = (i - x)*m_cell_size_x;
			double y_dist = (j - y)*m_cell_size_y;
			double dist = sqrt(pow(x_dist, 2.0) + pow(y_dist, 2.0));
			if (dist <= distance) dist = dist/distance;
			else dist = 1.0;
			double change = strength * (cos(dist * M_PI) + 1.0) / 2.0;
			m_u[i][j] -= change;
			change_sum += change;
		}

	change_sum /= (m_grid_x + 2)*(m_grid_y + 2);
	for (int i = 0; i < m_grid_x + 2; i++)
		for (int j = 0; j < m_grid_y + 2; j++) 
		{
			m_u[i][j] += change_sum;
		}
}
#include "water_surface.h"
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <windows.h>
#include "glplusx.h"
#include "glplusx_prog.h"
#include "sys_base.h"
#include "glext.h"
#define M_PI 3.14159265358979323846


WaterSurface::WaterSurface(
		float dim_x, float dim_z, float pos_y, int grid_x, int grid_z, 
		float wave_speed, float dt, float damp_factor, uint64 usec_step_time) 
{
	m_dim_x = dim_x;
	m_dim_z = dim_z;
	m_pos_y = pos_y;
	m_grid_x = grid_x;
	m_grid_z = grid_z;
	m_wave_speed = wave_speed;
	m_dt = dt;
	m_damp_factor = damp_factor;
	m_step = usec_step_time;

	m_plane = nullptr;
}

WaterSurface::~WaterSurface()
{
	if (m_plane != nullptr) 
		delete m_plane;
}

bool WaterSurface::init() 
{
	if (m_dim_x == 0 || m_dim_z == 0 || m_grid_x == 0 || m_grid_z == 0)
	{
		fprintf(stderr, "Invalid dimensions of water surface.\n");
		return false;
	}

	m_simulation_time = 0;
	m_last_call = 0;

	m_model_mat = math::Mat4x4f(math::Mat4x4f::I);
	
	m_plane = new Renderable();
	if (!m_plane->load_grid(m_dim_x/2.0f, m_dim_z/2.0f, m_pos_y, m_grid_x, m_grid_z, 1.0f, 1.0f))
	{
		fprintf(stderr, "Loading planes failed.\n");
		return false;
	}
	if (!m_plane->addTextures("base", L"data/textures/water_diff.jpg", nullptr, nullptr))
	{
		fprintf(stderr, "Loading texture for plane failed.\n");
		return false;
	}

	m_frame_buff.init();

	m_height_tex1.init();
	m_height_tex1.set_image(0, m_grid_x, m_grid_z, glp::Tex::IF_RGBA16F,
		glp::Tex::PF_RGBA, glp::Tex::PT_UNSIGNED_BYTE, nullptr);
	m_height_tex1.set_wrapST(glp::Tex::WrapMode::WM_CLAMP_TO_EDGE);
	// why CLAMP_TO_EDGE? We don't want linear interpolation beetweend two borders
	// what's more, we don't have to clamp manualy first and last row of texture,
	// but just use values < 0.0 and > 1.0

	m_height_tex2.init();
	m_height_tex2.set_image(0, m_grid_x, m_grid_z, glp::Tex::IF_RGBA16F,
		glp::Tex::PF_RGBA, glp::Tex::PT_UNSIGNED_BYTE, nullptr);
	m_height_tex2.set_wrapST(glp::Tex::WrapMode::WM_CLAMP_TO_EDGE);

	m_velocity_tex1.init();
	m_velocity_tex1.set_image(0, m_grid_x, m_grid_z, glp::Tex::IF_RGBA16F,
		glp::Tex::PF_RGBA, glp::Tex::PT_UNSIGNED_BYTE, nullptr);
	m_velocity_tex1.set_wrapST(glp::Tex::WrapMode::WM_CLAMP_TO_EDGE);

	m_velocity_tex2.init();
	m_velocity_tex2.set_image(0, m_grid_x, m_grid_z, glp::Tex::IF_RGBA16F,
		glp::Tex::PF_RGBA, glp::Tex::PT_UNSIGNED_BYTE, nullptr);
	m_velocity_tex2.set_wrapST(glp::Tex::WrapMode::WM_CLAMP_TO_EDGE);

	m_act_height_tex = &m_height_tex1;
	m_new_height_tex = &m_height_tex2;

	m_act_velocity_tex = &m_velocity_tex1;
	m_new_velocity_tex = &m_velocity_tex2;

	return init_render_programs();
}

bool WaterSurface::init_render_programs()
{
	// render shaders
	{
		glp::VertProgram vprog;
		vprog.init();
		glpx::program_set_source_file(vprog, "glsl/water_vprog.txt");
		if (!vprog.compile())
		{
			glpx::ProgramLog pl;
			MessageBoxA(NULL, glpx::get_log(vprog, pl),
				"VERTEX PROGRAM ERROR", MB_OK | MB_ICONSTOP);
			vprog.release();
			return false;
		}

		glp::FragProgram fprog;
		fprog.init();
		glpx::program_set_source_file(fprog, "glsl/water_fprog.txt");
		if (!fprog.compile())
		{
			glpx::ProgramLog pl;
			MessageBoxA(nullptr, glpx::get_log(fprog, pl),
				"FRAGMENT PROGRAM ERROR", MB_OK | MB_ICONSTOP);
			fprog.release();
			vprog.release();
			return false;
		}

		m_water_render_prog.init();
		m_water_render_prog.attach(vprog);
		m_water_render_prog.attach(fprog);

		m_water_render_prog.bind_attrib_loc("point", Renderable::ATTR_LOC_POINT);
		m_water_render_prog.bind_attrib_loc("coord", Renderable::ATTR_LOC_COORD);
		m_water_render_prog.bind_attrib_loc("normal", Renderable::ATTR_LOC_NORMAL);
		m_water_render_prog.bind_attrib_loc("tgtU", Renderable::ATTR_LOC_TGT_U);
		m_water_render_prog.bind_attrib_loc("tgtV", Renderable::ATTR_LOC_TGT_V);

		if (!m_water_render_prog.link())
		{
			glpx::ProgramLog pl;
			MessageBoxA(nullptr, glpx::get_log(m_water_render_prog, pl),
				"PROGRAM LINK ERROR", MB_OK | MB_ICONSTOP);
			m_water_render_prog.release();
			fprog.release();
			vprog.release();
			return false;
		}

		m_water_render_prog.uniform("texDiff", 0);
		m_water_render_prog.uniform("texNormal", 1);
		m_water_render_prog.uniform("texHeight", 2);
		m_water_render_prog.uniform_vec2("size", math::Vec2f(m_grid_x, m_grid_z).m);
		m_water_render_prog.uniform("h_x", m_dim_x / m_grid_x);
		m_water_render_prog.uniform("h_z", m_dim_z / m_grid_z);
	}

	// GPGPU shaders
	{
		glp::VertProgram vprog;
		vprog.init();
		glpx::program_set_source_file(vprog, "glsl/calc_wave_vprog.txt");
		if (!vprog.compile()) {
			glpx::ProgramLog pl;
			MessageBoxA(NULL, glpx::get_log(vprog, pl),
				"VERTEX PROGRAM ERROR", MB_OK | MB_ICONSTOP);
			vprog.release();
			return false;
		}

		glp::FragProgram fprog;
		fprog.init();
		glpx::program_set_source_file(fprog, "glsl/calc_wave_fprog.txt");
		if (!fprog.compile()) {
			glpx::ProgramLog pl;
			MessageBoxA(nullptr, glpx::get_log(fprog, pl),
				"FRAGMENT PROGRAM ERROR", MB_OK | MB_ICONSTOP);
			fprog.release();
			vprog.release();
			return false;
		}

		m_update_height_prog.init();
		m_update_height_prog.attach(vprog);
		m_update_height_prog.attach(fprog);

		m_update_height_prog.bind_attrib_loc("point", 0);

		m_update_height_prog.bind_frag_data_loc("height", 0);
		m_update_height_prog.bind_frag_data_loc("velocity", 1);

		if (!m_update_height_prog.link())
		{
			glpx::ProgramLog pl;
			MessageBoxA(nullptr, glpx::get_log(m_update_height_prog, pl),
				"PROGRAM LINK ERROR", MB_OK | MB_ICONSTOP);
			m_update_height_prog.release();
			fprog.release();
			vprog.release();
			return false;
		}

		m_update_height_prog.uniform("heightOld", 0);
		m_update_height_prog.uniform("velocityOld", 1);
		m_update_height_prog.uniform_vec2("size", math::Vec2f(m_grid_x, m_grid_z).m);
		m_update_height_prog.uniform("h_x", m_dim_x / m_grid_x);
		m_update_height_prog.uniform("h_z", m_dim_z / m_grid_z);
		m_update_height_prog.uniform("wave_speed", m_wave_speed);
		m_update_height_prog.uniform("dt", m_dt);
		m_update_height_prog.uniform("damp_factor", m_damp_factor);

		math::Vec2f quad[4] =
		{
			math::Vec2f(-1.0f, -1.0f),
			math::Vec2f( 1.0f, -1.0f),
			math::Vec2f(-1.0f,  1.0f),
			math::Vec2f( 1.0f,  1.0f)
		};
		m_quad.init();
		m_quad.buffer_data(4*sizeof(math::Vec2f), glp::Buffer::UM_STATIC_DRAW, quad);

		m_varray.init();
		glp::Device::bind_vertex_array(m_varray);
		glp::Device::bind_buffer(m_quad);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_TRUE, sizeof(math::Vec2f), nullptr);
		glp::Device::unbind_vertex_array(m_varray);
		glp::Device::unbind_buffer(m_quad);
	}
	return true;
}

void WaterSurface::render(
	const math::Vec3f viewer_pos, const math::Mat4x4f projection,
	const math::Mat4x4f& inv_view, const math::Mat4x4f& rot_inv,
	const glp::TexCube &cube_map)
{
	glp::Device::bind_program(m_water_render_prog);
	
	m_water_render_prog.uniform_mat4x4("proj", projection.m, true);
	m_water_render_prog.uniform_mat4x4("rot_inv", rot_inv.m, true);
	m_water_render_prog.uniform_vec3("viewerPos", viewer_pos.m);

	glp::Device::bind_tex(*m_act_height_tex, 4);
	glp::Device::bind_tex(cube_map, 5);
	m_water_render_prog.uniform("wave_height", 4);
	m_water_render_prog.uniform("cube_map", 5);
	m_water_render_prog.uniform_mat4x4("model", m_model_mat.m, true);
	m_water_render_prog.uniform_mat4x4("modelView", (inv_view*m_model_mat).m, true);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	m_plane->render(true);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glp::Device::unbind_tex(cube_map, 5);
	glp::Device::unbind_tex(*m_act_height_tex, 4);
	glp::Device::unbind_program(m_water_render_prog);
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

		// render heights (and normals in the future) to texture
		glp::Device::bind_program(m_update_height_prog);

		m_frame_buff.attach_tex_2d(*m_new_height_tex, 0);
		m_frame_buff.attach_tex_2d(*m_new_velocity_tex, 1);
	
		glp::Device::bind_fbuff(m_frame_buff);

		GLenum bufs[2] = {
			GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
		glDrawBuffers(2, bufs);

		glClear(GL_COLOR_BUFFER_BIT);
	
		glp::Device::bind_tex(*m_act_height_tex, 0);
		glp::Device::bind_tex(*m_act_velocity_tex, 1);
		
		glp::Device::bind_vertex_array(m_varray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glp::Device::unbind_vertex_array(m_varray);

		glp::Device::unbind_tex(*m_act_velocity_tex, 1);
		glp::Device::unbind_tex(*m_act_height_tex, 0);

		glp::Device::unbind_fbuff(m_frame_buff);

		m_frame_buff.detach_tex_2d(1);
		m_frame_buff.detach_tex_2d(0);

		m_new_height_tex->gen_mipmaps();
		m_new_velocity_tex->gen_mipmaps();

		// swap textures
		glp::Tex2D* tmp = m_act_height_tex;
		m_act_height_tex = m_new_height_tex;
		m_new_height_tex = tmp;

		tmp = m_act_velocity_tex;
		m_act_velocity_tex = m_new_velocity_tex;
		m_new_velocity_tex = tmp;
	}
}

void WaterSurface::touch(int x, int y, double strength, double distance)
{
	m_update_height_prog.uniform("touch_distance", float(distance));
	m_update_height_prog.uniform("touch_strength", float(strength));
	m_update_height_prog.uniform_vec2("touch_pos", math::Vec2f(x, y).m);

	update_model(0, true);

	m_update_height_prog.uniform("touch_distance", 0.0f);
}

float WaterSurface::get_dim_x()
{
	return m_dim_x;
}
float WaterSurface::get_dim_z()
{
	return m_dim_z;
}
float WaterSurface::get_pos_y()
{
	return m_pos_y;
}
int WaterSurface::get_grid_x()
{
	return m_grid_x;
}
int WaterSurface::get_grid_z()
{
	return m_grid_z;
}
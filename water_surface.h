#ifndef watersurfaceH
#define watersurfaceH

#include "renderable.h"
#include "glplus.h"

class WaterSurface
{
public:
	WaterSurface(
		float dim_x, float dim_z, int grid_x, int grid_z, 
		float wave_speed, float dt, float damp_factor, uint64 usec_step_time);
	bool init();
	void render(
		glp::Program& render_program, 
		const math::Mat4x4f& inv_view) const;
	void update_model(uint64 usec_time, bool force_one_step);
	void touch(int x, int y, double strength, double distance);
	~WaterSurface();

private:
	// set by constructor
	float m_dim_x;
	float m_dim_y;
	int m_grid_x;
	int m_grid_y;
	double m_wave_speed;
	double m_dt;
	double m_damp_factor;
	uint64 m_step;

	// initialized in the init() method
	float m_cell_size_x;
	float m_cell_size_y;
	double** m_u;
	double** m_u_new;
	double** m_v;
	uint64 m_simulation_time;
	uint64 m_last_call;
	//
	Renderable* m_bar;
	math::Mat4x4f** m_model_mat;
};

#endif
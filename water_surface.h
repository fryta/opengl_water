#ifndef watersurfaceH
#define watersurfaceH

#include "renderable.h"
#include "glplus.h"

class WaterSurface
{
public:
	WaterSurface() {}
	void set_dim(float dim_x, float dim_y);
	void set_grid_dim(int grid_x, int grid_y);
	bool init();
	void render(
		glp::Program& render_program, 
		const math::Mat4x4f& inv_view) const;
	void update_model_simple(uint64 usec_time, bool force_one_step);
	void update_model(uint64 usec_time, bool force_one_step);
	void touch(int x, int y, double strength, double distance);

private:
	float m_dim_x;
	float m_dim_y;
	int m_grid_x;
	int m_grid_y;
	float m_cell_size_x;
	float m_cell_size_y;
	double m_wave_speed;
	double m_dt;
	double m_damp_factor;
	double** m_u;
	double** m_u_new;
	double** m_v;
	uint64 m_simulation_time;
	uint64 m_last_call;
	uint64 m_step;
	Renderable* m_bar;
	math::Mat4x4f** m_model_mat;

	void preserve_avg_height();
};

#endif
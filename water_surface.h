#ifndef watersurfaceH
#define watersurfaceH

#include "renderable.h"
#include "glplus.h"

class WaterSurface
{
public:
	WaterSurface(
		float dim_x, float dim_z, float pos_y, int grid_x, int grid_z, 
		float wave_speed, float dt, float damp_factor, uint64 usec_step_time);
	bool init();
	void render(
		const math::Vec3f viewer_pos, const math::Mat4x4f projection, 
		const math::Mat4x4f& inv_view,
		const glp::TexCube &cube_map, const glp::TexCube &pool_map);
	void update_model(uint64 usec_time, bool force_one_step);
	void touch(int x, int y, double strength, double distance);
	~WaterSurface();

	float get_dim_x();
	float get_dim_z();
	float get_pos_y();
	int get_grid_x();
	int get_grid_z();

private:
	bool init_render_programs();
	// set by constructor
	float m_dim_x;
	float m_dim_z;
	float m_pos_y;
	int m_grid_x;
	int m_grid_z;
	float m_wave_speed;
	float m_dt;
	float m_damp_factor;
	uint64 m_step;

	// water and air refract indexes
	float m_air_refract_index;
	float m_water_refract_index;

	// initialized in the init() method
	float m_cell_size_x;
	float m_cell_size_y;
	uint64 m_simulation_time;
	uint64 m_last_call;

	math::Mat4x4f m_model_mat;
	math::Mat4x4f m_caustics_model_mat;
	Renderable* m_plane;

	// render program
	glp::Program m_water_render_prog;
	glp::Program m_caustics_prog;

	glp::FrameBuffer m_frame_buff;
	//glp::RenderBuffer m_render_buff;
	glp::VertexBuffer m_quad;
	glp::VertexArray m_varray;
	// GPGPU program
	glp::Program m_update_height_prog;
	// textures for GPGPU calculations
	glp::Tex2D m_height_tex1; // during generation of new heigh map
	glp::Tex2D m_height_tex2; // old values are needed
	glp::Tex2D* m_act_height_tex;
	glp::Tex2D* m_new_height_tex;
	glp::Tex2D m_velocity_tex1;
	glp::Tex2D m_velocity_tex2;
	glp::Tex2D* m_act_velocity_tex;
	glp::Tex2D* m_new_velocity_tex;

	glp::Tex2D m_sunlight_tex;
};

#endif
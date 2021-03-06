#ifndef mainH
#define mainH

#include "sys_base.h"
#include "sys_window.h"
#include "glplus.h"
#include "renderable.h"
#include "water_surface.h"
#include "water_surface_cpu.h"


class MainForm: public sys::AppWindow
{
public:
	MainForm(const wchar_t* caption, int l, int t, int w, int h):
		sys::AppWindow(caption, false, &sys::Window::Position(l, t, w, h)),
		m_width(1), m_height(1), m_fov(0.0625f), m_wheelFov(0),
		m_cameraRotX(-0.3f), m_cameraRotY(-0.70f), m_cameraPos(3.0f, 2.0f, -4.0f) {}
		//m_cameraRotX(-0.0f), m_cameraRotY(-0.0f), m_cameraPos(0.0f, 0.0f, 0.0f) {}
	bool init();
	void release();

	virtual void on_clock(uint64 usecTime) override;
	virtual void on_size(int width, int height) override;

	virtual void on_mouse_move(int xPos, int yPos, uint key_flags) override;
	virtual void on_mouse_wheel(int xPos, int yPos, int delta, uint key_flags) override;
	virtual void on_mouse_lbtn_down(int xPos, int yPos, uint key_flags) override;
	virtual void on_mouse_lbtn_up(int xPos, int yPos, uint key_flags) override;
	virtual void on_mouse_rbtn_down(int xPos, int yPos, uint key_flags) override;
	virtual void on_mouse_rbtn_up(int xPos, int yPos, uint key_flags) override;

	virtual void on_key_down(int vKey, uint repCnt) override;
	virtual void on_key_up(int vKey) override;
	virtual void on_key_char(wchar_t chr, uint repCnt) override;

	virtual bool on_close() override;
	virtual void on_destroy() override;

private:
	void update(uint64 usecTime);
	void update(uint64 usecTime, bool renderWater);
	void map_mouse_click_on_plane(int x_pos, int y_pos, float plane_y, float &word_x, float &word_z);
	//void create_poolbox_cubemap();

	int m_width;
	int m_height;

	glp::Device m_dev;
	glp::Program m_renderProg;
	glp::Program m_skybox_prog;
	glp::TimerQuery m_timerQuery;
	float m_displFreq;

	// Scene object data
	stx::vector<Renderable*> m_objects;
	stx::vector<std::pair<math::Mat4x4f, Renderable*>> m_instances;
	WaterSurface* m_water;
	Renderable* m_skybox;
	glp::TexCube m_skybox_cubemap;

	// Camera data
	float m_tmpTrackRotX;
	float m_tmpTrackRotY;
	math::Vec3f m_tmpTrackPos;
	float m_cameraRotX;
	float m_cameraRotY;
	math::Vec3f m_cameraPos;
	float m_fov;
	int m_wheelFov;

	math::Mat4x4f m_proj;
	bool m_queryStarted;
	/*
	glp::FrameBuffer m_frame_buff;
	glp::RenderBuffer m_render_buff;
	FILE * m_logFile;*/
};


#endif

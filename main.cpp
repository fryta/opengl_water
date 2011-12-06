#include <windows.h>
#include <GdiPlus.h>
#include <assert.h>
#include "main.h"
#include "glplus.h"
#include "glplusx.h"
#include "glplusx_prog.h"
#include "sys_base.h"
#include "mathx_quaternion.h"
#include "mathx_vector.h"
#include "glext.h"

#pragma comment(lib, "GdiPlus.lib")
#pragma comment(lib, "opengl32.lib")



int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
	ULONG_PTR gdipToken;
	Gdiplus::GdiplusStartupInput gsi;
	Gdiplus::GdiplusStartup(&gdipToken, &gsi, NULL);

	WPARAM rslt = 1;
	{
		MainForm form(L"OpenGL Rendering Framework",
			100, 100, 1400, 900);
		if (!form.init())
			return 1;

		rslt = form.main_loop();
		form.release();
	}

	Gdiplus::GdiplusShutdown(gdipToken);
	return int(rslt);
}


bool MainForm::init()
{
	if (!m_dev.init(handle(), 3, 3, 24, 8, 24, 0, 4))
	{
		message_box(L"OpenGL initialization failed.",
			L"CATASTROPHIC ERROR", MB_OK | MB_ICONSTOP);
		return false;
	}

	{
		glp::VertProgram vprog;
		vprog.init();
		glpx::program_set_source_file(vprog, "glsl/illum_vprog.txt");
		if (!vprog.compile())
		{
			glpx::ProgramLog pl;
			MessageBoxA(NULL, glpx::get_log(vprog, pl),
				"VERTEX PROGRAM ERROR", MB_OK | MB_ICONSTOP);
			vprog.release();
			m_dev.release();
			return false;
		}

		glp::FragProgram fprog;
		fprog.init();
		glpx::program_set_source_file(fprog, "glsl/illum_fprog.txt");
		if (!fprog.compile())
		{
			glpx::ProgramLog pl;
			MessageBoxA(nullptr, glpx::get_log(fprog, pl),
				"FRAGMENT PROGRAM ERROR", MB_OK | MB_ICONSTOP);
			fprog.release();
			vprog.release();
			m_dev.release();
			return false;
		}

		m_renderProg.init();
		m_renderProg.attach(vprog);
		m_renderProg.attach(fprog);

		m_renderProg.bind_attrib_loc("point", Renderable::ATTR_LOC_POINT);
		m_renderProg.bind_attrib_loc("coord", Renderable::ATTR_LOC_COORD);
		m_renderProg.bind_attrib_loc("normal", Renderable::ATTR_LOC_NORMAL);
		m_renderProg.bind_attrib_loc("tgtU", Renderable::ATTR_LOC_TGT_U);
		m_renderProg.bind_attrib_loc("tgtV", Renderable::ATTR_LOC_TGT_V);

		if (!m_renderProg.link())
		{
			glpx::ProgramLog pl;
			MessageBoxA(nullptr, glpx::get_log(m_renderProg, pl),
				"PROGRAM LINK ERROR", MB_OK | MB_ICONSTOP);
			m_renderProg.release();
			fprog.release();
			vprog.release();
			m_dev.release();
			return false;
		}
	}

	Renderable* ren = nullptr;

	/*
	ren = new Renderable();
	if (!ren->load_plane(0.1f, 0.1f, -0.1f, 8.0f, 8.0f))
		return false;
	if (!ren->addTextures("base", L"../data/GR_GK_011_diffuse.jpg",
		L"../data/GR_GK_011_normal.jpg", L"../data/GR_GK_011_height.jpg"))
		return false;

	m_objects.push_back(ren);
	m_instances.push_back(std::make_pair(math::Mat4x4f(math::Mat4x4f::I), ren));
	*/

	ren = new Renderable();
	if (!ren->load_obj(L"../data/pool3.obj.txt", false, false))
		return false;
	if (!ren->addTextures("base", L"../data/simple_diff.jpg", nullptr, nullptr))
		return false;
	/*
	if (!ren->addTextures("base", L"../data/stones_diffuse.png",
		L"../data/stones_normal.png", L"../data/stones_height.png"))
		return false;
	*/
	m_objects.push_back(ren);
	m_instances.push_back(std::make_pair(math::Mat4x4f(math::Mat4x4f::I), ren));

	m_water.set_dim(8.0f, 4.0f);
	m_water.set_grid_dim(100, 50);
	m_water.init();
	//m_water.touch(32, 16, 0.3, 1.0);

	glp::Device::enable_multisample();
	glClearDepth(1.0);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0);
	assert(glGetError() == GL_NO_ERROR);
	show();
	return true;
}

void MainForm::release()
{
	for (size_t a = 0; a < m_objects.size(); ++a)
	{
		m_objects[a]->release();
		delete m_objects[a];
	}
	m_renderProg.release();
	m_dev.release();
}



void MainForm::on_clock(uint64 usecTime)
{
	update(usecTime);
	assert(glGetError() == GL_NO_ERROR);
	m_dev.swap_buffers();
}

void MainForm::on_size(int width, int height)
{
	if (width <= 0 || height <= 0) return;
	m_width = width;
	m_height = height;
	glViewport(0, 0, width, height);

	math::set_projection(m_proj,
		math::Vec2f(-m_fov*float(width)/float(height), -m_fov),
		math::Vec2f( m_fov*float(width)/float(height),  m_fov),
		0.125f, 256.0f);

	ASSERT(glGetError() == GL_NO_ERROR);
}



void MainForm::on_mouse_move(int xPos, int yPos, uint key_flags)
{
	if (key_flags & MK_RBUTTON)
	{
		math::Vec3f curr(float(xPos), float(yPos), -1.0f);
		curr.x = (curr.x / m_width)*2.0f - 1.0f;
		curr.y = -(curr.y / m_height)*2.0f + 1.0f;

		float rotY = float(xPos)/float(m_width)*2.0f - 1.0f - m_tmpTrackPos.x;
		float rotX = -float(yPos)/float(m_height)*2.0f + 1.0f - m_tmpTrackPos.y;
		m_cameraRotX = fmodf(m_tmpTrackRotX + rotX, math::consts<float>::_2pi);
		m_cameraRotY = fmodf(m_tmpTrackRotY + rotY, math::consts<float>::_2pi);
	}
}

void MainForm::on_mouse_wheel(int xPos, int yPos, int delta, uint key_flags)
{
	if (delta < 0) ++m_wheelFov;
	if (delta > 0) --m_wheelFov;
	m_fov = 0.0625f*math::pow2(0.03125f*float(m_wheelFov));

	math::set_projection(m_proj,
		math::Vec2f(-m_fov*float(m_width)/float(m_height), -m_fov), 
		math::Vec2f( m_fov*float(m_width)/float(m_height),  m_fov),
		0.125f, 256.0f);
}

void MainForm::on_mouse_lbtn_down(int xPos, int yPos, uint key_flags)
{
}

void MainForm::on_mouse_lbtn_up(int xPos, int yPos, uint key_flags)
{
}

void MainForm::on_mouse_rbtn_down(int xPos, int yPos, uint key_flags)
{
	m_tmpTrackPos.x = (float(xPos)/float(m_width))*2.0f - 1.0f;
	m_tmpTrackPos.y = -(float(yPos)/float(m_height))*2.0f + 1.0f;
	m_tmpTrackPos.z = -1.0f;
	m_tmpTrackRotX = m_cameraRotX;
	m_tmpTrackRotY = m_cameraRotY;
}

void MainForm::on_mouse_rbtn_up(int xPos, int yPos, uint key_flags)
{
}



void MainForm::on_key_down(int vKey, uint repCnt)
{
	math::Mat3x3f matCameraRot =
		math::Mat3x3f(
		cosf(m_cameraRotY), 0, sinf(m_cameraRotY),
		                 0, 1, 0,
		-sinf(m_cameraRotY), 0, cosf(m_cameraRotY))
		*math::Mat3x3f(
			1,                  0, 0,
			0, cosf(m_cameraRotX), sinf(m_cameraRotX),
			0, -sinf(m_cameraRotX), cosf(m_cameraRotX));

	math::Vec3f offs(0.0f);
	if (vKey == 'D') offs.x += 0.125f;
	if (vKey == 'A') offs.x -= 0.125f;
	if (vKey == 'Q') offs.y += 0.125f;
	if (vKey == 'E') offs.y -= 0.125f;
	if (vKey == 'W') offs.z += 0.125f;
	if (vKey == 'S') offs.z -= 0.125f;
	if (vKey == 'T') m_water.touch(50, 25, 0.3, 1.0);
	m_cameraPos += matCameraRot*offs;
}

void MainForm::on_key_up(int vKey)
{
}

void MainForm::on_key_char(wchar_t chr, uint repCnt)
{
}



bool MainForm::on_close()
{
	return true;
}

void MainForm::on_destroy()
{
	quit_main_loop();
}


void MainForm::update(uint64 usecTime)
{
	glp::Device::bind_program(m_renderProg);

	glp::Device::enable_depth_test();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	math::Mat4x4f invView;
	math::set_translation(invView, -m_cameraPos);
	math::rotate(invView, 0, 2, -m_cameraRotY);
	math::rotate(invView, 1, 2, -m_cameraRotX);

	m_renderProg.uniform_mat4x4("proj", m_proj.m, true);
	m_renderProg.uniform_vec3("viewerPos", m_cameraPos.m);

	for (size_t a = 0; a < m_instances.size(); ++a)
	{
		/*
		if (a == 1)
			math::set_rotation(m_instances[a].first, 0, 2,
				float((usecTime >> 8)&0xffff)/(65536.0f)*math::consts<float>::_2pi);
		*/

		m_renderProg.uniform_mat4x4("model", m_instances[a].first.m, true);
		m_renderProg.uniform_mat4x4("modelView", (invView*m_instances[a].first).m, true);
		m_instances[a].second->render(true);
	}

	m_water.update_model(usecTime, false);
	m_water.render(m_renderProg, invView);
	

	glp::Device::disable_depth_test();
}

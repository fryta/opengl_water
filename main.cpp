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

	DEVMODE dm;
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm);
	m_displFreq = dm.dmDisplayFrequency;

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

		m_renderProg.uniform("texDiff", 0);
		m_renderProg.uniform("texNormal", 1);
		m_renderProg.uniform("texHeight", 2);
	}

	{
		glp::VertProgram vprog;
		vprog.init();
		glpx::program_set_source_file(vprog, "glsl/skybox.vp");
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
		glpx::program_set_source_file(fprog, "glsl/skybox.fp");
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

		m_skybox_prog.init();
		m_skybox_prog.attach(vprog);
		m_skybox_prog.attach(fprog);

		m_skybox_prog.bind_attrib_loc("point", Renderable::ATTR_LOC_POINT);

		if (!m_skybox_prog.link())
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

	m_timerQuery.init();
	m_queryStarted = false;

	//################## Renderable objects
	Renderable* ren = nullptr;

	ren = new Renderable();
	if (!ren->load_obj(L"data/objects/pool3.obj.txt", false, false))
		return false;
	if (!ren->addTextures("base", L"data/textures/simple_diff.jpg", nullptr, nullptr))
		return false;
		
	m_objects.push_back(ren);
	m_instances.push_back(std::make_pair(math::Mat4x4f(math::Mat4x4f::I), ren));

	ren = new Renderable();
	if (!ren->load_obj(L"data/objects/ter2.obj.txt", false, false))
		return false;
	if (!ren->addTextures("base", L"data/textures/simple_diff.jpg", nullptr, nullptr))
		return false;

	m_objects.push_back(ren);
	m_instances.push_back(std::make_pair(math::Mat4x4f(math::Mat4x4f::I), ren));

	m_water = new WaterSurface(8.0f, 4.0f, -0.07f, 400, 200, 0.4f, 0.01f, 0.995f, 10000);
	if(!m_water->init())
		return false;

	m_skybox = new Renderable();
	if (!m_skybox->load_box(128.0f, 128.0f, 128.0f))
		return false;
	// TODO: change lines below
	if (!m_skybox->addTextures("base", L"data/textures/water_diff.jpg", nullptr, nullptr))
		return false;

	//################## Textures
	m_skybox_cubemap.init();
	if(!glpx::LoadTexCube_RGBA(m_skybox_cubemap, glp::TexCubeBase::CF_X_POS, L"data/textures/skybox/vanilla_sky_lf.jpg"))
		return false;
	if(!glpx::LoadTexCube_RGBA(m_skybox_cubemap, glp::TexCubeBase::CF_X_NEG, L"data/textures/skybox/vanilla_sky_rt.jpg"))
		return false;
	if(!glpx::LoadTexCube_RGBA(m_skybox_cubemap, glp::TexCubeBase::CF_Y_POS, L"data/textures/skybox/vanilla_sky_up.jpg"))
		return false;
	if(!glpx::LoadTexCube_RGBA(m_skybox_cubemap, glp::TexCubeBase::CF_Y_NEG, L"data/textures/skybox/vanilla_sky_dn.jpg"))
		return false;
	if(!glpx::LoadTexCube_RGBA(m_skybox_cubemap, glp::TexCubeBase::CF_Z_POS, L"data/textures/skybox/vanilla_sky_ft.jpg"))
		return false;
	if(!glpx::LoadTexCube_RGBA(m_skybox_cubemap, glp::TexCubeBase::CF_Z_NEG, L"data/textures/skybox/vanilla_sky_bk.jpg"))
		return false;
	
	m_skybox_cubemap.set_wrapSTR(glp::Tex::WrapMode::WM_CLAMP_TO_EDGE);
	m_skybox_cubemap.gen_mipmaps();


	glp::Device::enable_cubemap_seamless();

	glp::Device::enable_multisample();
	glClearDepth(1.0);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0);
	assert(glGetError() == GL_NO_ERROR);

	show();

	return true;
}
/*
void MainForm::create_poolbox_cubemap(){
	math::Vec3f tmpCameraPos = m_cameraPos;
	float tmpRotX = m_cameraRotX;
	float tmpRotY = m_cameraRotY;

	m_cameraPos = math::Vec3f(0.0f, -1.0f, 0.0f);

	m_cameraRotX = 0.0f;
	m_cameraRotY = fmodf(0.5f * math::consts<float>::_2pi, math::consts<float>::_2pi);
	//m_cameraRotY = fmodf(0.25f * math::consts<float>::_2pi, math::consts<float>::_2pi);
	//m_cameraRotY = fmodf(-0.25f * math::consts<float>::_2pi, math::consts<float>::_2pi);
	//m_cameraRotY = fmodf(-0.5f * math::consts<float>::_2pi, math::consts<float>::_2pi);

	glp::Device::bind_program(m_renderProg);
	ASSERT(glGetError() == GL_NO_ERROR);

	// create and bind Framebuffer object
	m_frame_buff.init();
	ASSERT(glGetError() == GL_NO_ERROR);

	// create and bind pool cubemap 
	m_poolbox_cubemap.init();
	glp::Device::bind_tex(m_poolbox_cubemap);
	ASSERT(glGetError() == GL_NO_ERROR);

	m_frame_buff.attach_tex_face(m_poolbox_cubemap, glp::TexCubeBase::CF_X_POS);
	ASSERT(glGetError() == GL_NO_ERROR);

	glp::Device::bind_fbuff(m_frame_buff);
	ASSERT(glGetError() == GL_NO_ERROR);

	glClear(GL_COLOR_BUFFER_BIT); 
	ASSERT(glGetError() == GL_NO_ERROR);
	//update(0, false);

	glp::Device::unbind_fbuff(m_frame_buff);
	ASSERT(glGetError() == GL_NO_ERROR);

	//m_frame_buff.detach_tex_face(glp::TexCubeBase::CF_X_POS);
	//int error = glGetError();
	//fprintf(m_logFile, "%d\n", error);
	//ASSERT(error == GL_NO_ERROR);

	glp::Device::unbind_tex(m_poolbox_cubemap);
	ASSERT(glGetError() == GL_NO_ERROR);

	glp::Device::unbind_program(m_renderProg);
	ASSERT(glGetError() == GL_NO_ERROR);
	
	
	
	//if(!glpx::LoadTexCube_RGBA(m_poolbox_cubemap, glp::TexCubeBase::CF_X_POS, L"data/textures/cubemap_grid_256.png"))
	//	return false;
	//if(!glpx::LoadTexCube_RGBA(m_poolbox_cubemap, glp::TexCubeBase::CF_X_NEG, L"data/textures/cubemap_grid_256.png"))
	//	return false;
	//if(!glpx::LoadTexCube_RGBA(m_poolbox_cubemap, glp::TexCubeBase::CF_Y_POS, L"data/textures/cubemap_grid_256.png"))
	//	return false;
	//if(!glpx::LoadTexCube_RGBA(m_poolbox_cubemap, glp::TexCubeBase::CF_Y_NEG, L"data/textures/cubemap_grid_256.png"))
	//	return false;
	//if(!glpx::LoadTexCube_RGBA(m_poolbox_cubemap, glp::TexCubeBase::CF_Z_POS, L"data/textures/cubemap_grid_256.png"))
	//	return false;
	//if(!glpx::LoadTexCube_RGBA(m_poolbox_cubemap, glp::TexCubeBase::CF_Z_NEG, L"data/textures/cubemap_grid_256.png"))
	//	return false;
	

	m_poolbox_cubemap.set_wrapSTR(glp::Tex::WrapMode::WM_CLAMP_TO_EDGE);
	m_poolbox_cubemap.gen_mipmaps();
	ASSERT(glGetError() == GL_NO_ERROR);

	m_cameraPos = tmpCameraPos;
	m_cameraRotX = tmpRotX;
	m_cameraRotY = tmpRotY;
}*/

void MainForm::release()
{
	for (size_t a = 0; a < m_objects.size(); ++a)
	{
		m_objects[a]->release();
		delete m_objects[a];
	}
	delete m_water;
	delete m_skybox;
	m_renderProg.release();
	m_dev.release();

	m_skybox_cubemap.release();
}



void MainForm::on_clock(uint64 usecTime)
{
	int64 gpuTime = 0;
	bool rsltAvailable = false;

	if (m_queryStarted)
		rsltAvailable = m_timerQuery.resultAvailable();

	if (rsltAvailable)
		gpuTime = m_timerQuery.result_i64();

	if (!m_queryStarted || rsltAvailable)
		m_timerQuery.begin();

	update(usecTime);
	assert(glGetError() == GL_NO_ERROR);

	if (!m_queryStarted || rsltAvailable)
		m_timerQuery.end();

	m_dev.swap_buffers();

	m_queryStarted = true;

	if (rsltAvailable)
	{
		float gpuLoad = float(gpuTime)*float(m_displFreq)*1.0e-9f;

		wchar_t buff[128];
		swprintf(buff, L"OpenGL Rendering Framework, GPU load: %4.1f%%", gpuLoad*100.0f);
		SetWindowText((HWND)handle(), buff);
	}
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

void MainForm::map_mouse_click_on_plane(int x, int y, float plane_y, float &word_x, float &word_z)
{
	float x_pos = (float(x) / m_width)*2.0f - 1.0f;
	float y_pos = (float(m_height - y) / m_height)*2.0f - 1.0f;
	math::Vec4f ray = math::Vec4f(x_pos, y_pos, 1.0f, 0.0f);
	ray.x /= m_proj.m[0];
	ray.y /= m_proj.m[5];

	math::Mat4x4f invView;
	math::set_translation(invView, -m_cameraPos);
	math::rotate(invView, 0, 2, -m_cameraRotY);
	math::rotate(invView, 1, 2, -m_cameraRotX);
	invView = math::invert(invView);

	ray = invView * ray;
	math::Vec3f n_ray = math::Vec3f(ray.x, ray.y, ray.z);
	math::Vec3f n = math::Vec3f(0.0f, 1.0f, 0.0f);
	math::Vec3f plane_point = math::Vec3f(0.0f, plane_y, 0.0f);

	float d = math::dot_product(plane_point - m_cameraPos, n) / math::dot_product(n_ray, n);
	word_x = d*ray.x + m_cameraPos.x;
	word_z = d*ray.z + m_cameraPos.z;
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
	if (key_flags & MK_LBUTTON)
	{
		float x, z;
		map_mouse_click_on_plane(xPos, yPos, m_water->get_pos_y(), x, z);
		int grid_x = (x + m_water->get_dim_x()/2.0)/m_water->get_dim_x() * m_water->get_grid_x();
		int grid_z = (z + m_water->get_dim_z()/2.0)/m_water->get_dim_z() * m_water->get_grid_z();
		m_water->touch(grid_x, grid_z, 0.01, 10.0);
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
	float x, z;
	map_mouse_click_on_plane(xPos, yPos, m_water->get_pos_y(), x, z);
	int grid_x = (x + m_water->get_dim_x()/2.0)/m_water->get_dim_x() * m_water->get_grid_x();
	int grid_z = (z + m_water->get_dim_z()/2.0)/m_water->get_dim_z() * m_water->get_grid_z();
	m_water->touch(grid_x, grid_z, 0.04, 10.0);
	
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
	if (vKey == 'T') m_water->touch(rand() % m_water->get_grid_x(), rand() % m_water->get_grid_z(), 0.07, 4.0 + (rand() % 300)/100.0);
	
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
	ASSERT(glGetError() == GL_NO_ERROR);

	math::Mat4x4f invView;
	math::set_translation(invView, -m_cameraPos);
	math::rotate(invView, 0, 2, -m_cameraRotY);
	math::rotate(invView, 1, 2, -m_cameraRotX);
	
	m_renderProg.uniform_mat4x4("proj", m_proj.m, true);
	m_renderProg.uniform_vec3("viewerPos", m_cameraPos.m);

	for (size_t a = 0; a < m_instances.size(); ++a)
	{
		m_renderProg.uniform_mat4x4("model", m_instances[a].first.m, true);
		m_renderProg.uniform_mat4x4("modelView", (invView*m_instances[a].first).m, true);
		m_instances[a].second->render(true);
	}
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	m_water->render(m_cameraPos, m_proj, invView, m_skybox_cubemap);
	
	glp::Device::bind_program(m_skybox_prog);
	glp::Device::bind_tex(m_skybox_cubemap, 3);
	m_skybox_prog.uniform("cubeMap", 3);
	m_skybox_prog.uniform_mat4x4("proj", m_proj.m, true);
	math::Mat4x4f rot_only_view = math::Mat4x4f(math::Mat4x4f::I);
	math::rotate(rot_only_view, 0, 2, -m_cameraRotY);
	math::rotate(rot_only_view, 1, 2, -m_cameraRotX);
	m_skybox_prog.uniform_mat4x4("model_view", rot_only_view.m, true);
	m_skybox->render(true);
	glp::Device::unbind_tex(m_skybox_cubemap, 3);
	
	glp::Device::disable_depth_test();

	m_water->update_model(usecTime, false);
}

void generate_skybox()
{

}
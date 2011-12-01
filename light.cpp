#include <assert.h>
#include <windows.h>
#include "light.h"
#include "glplusx_prog.h"


bool ShadowMap::init(int resolution)
{
	m_resolution = resolution;

	glp::VertProgram shadow_vprog;
	shadow_vprog.init();
	glpx::program_set_source_file(shadow_vprog, "glsl/shadow_vprog.txt");
	if (!shadow_vprog.compile())
	{
		glpx::ProgramLog pl;
		MessageBoxA(NULL, glpx::get_log(shadow_vprog, pl),
			"VERTEX PROGRAM ERROR", MB_OK | MB_ICONSTOP);
		return false;
	}

	glp::FragProgram shadow_fprog;
	shadow_fprog.init();
	glpx::program_set_source_file(shadow_fprog, "glsl/shadow_fprog.txt");
	if (!shadow_fprog.compile())
	{
		glpx::ProgramLog pl;
		MessageBoxA(NULL, glpx::get_log(shadow_fprog, pl),
			"FRAGMENT PROGRAM ERROR", MB_OK | MB_ICONSTOP);
		return false;
	}

	m_shadowProg.init();
	m_shadowProg.attach(shadow_vprog);
	m_shadowProg.attach(shadow_fprog);
	m_shadowProg.bind_attrib_loc("point", Renderable::ATTR_LOC_POINT);
	m_shadowProg.bind_frag_data_loc("lightDist", 0);
	if (!m_shadowProg.link())
	{
		glpx::ProgramLog pl;
		MessageBoxA(NULL, glpx::get_log(m_shadowProg, pl),
			"PROGRAM LINK ERROR", MB_OK | MB_ICONSTOP);
		return false;
	}

	return true;
}

void ShadowMap::release()
{
	m_shadowProg.release();
}


bool PointShadowMap::init(int resolution)
{
	if (!ShadowMap::init(resolution))
		return false;

	m_shadowMap.init();
	m_shadowMap.set_wrapST(glp::Tex::WM_CLAMP_TO_EDGE);
	m_shadowMap.set_image(0, resolution, glp::TexCube::CF_X_POS,
		glp::Tex::IF_RG32F, glp::Tex::PF_RG, glp::Tex::PT_FLOAT, NULL);
	m_shadowMap.set_image(0, resolution, glp::TexCube::CF_X_NEG,
		glp::Tex::IF_RG32F, glp::Tex::PF_RG, glp::Tex::PT_FLOAT, NULL);
	m_shadowMap.set_image(0, resolution, glp::TexCube::CF_Y_POS,
		glp::Tex::IF_RG32F, glp::Tex::PF_RG, glp::Tex::PT_FLOAT, NULL);
	m_shadowMap.set_image(0, resolution, glp::TexCube::CF_Y_NEG,
		glp::Tex::IF_RG32F, glp::Tex::PF_RG, glp::Tex::PT_FLOAT, NULL);
	m_shadowMap.set_image(0, resolution, glp::TexCube::CF_Z_POS,
		glp::Tex::IF_RG32F, glp::Tex::PF_RG, glp::Tex::PT_FLOAT, NULL);
	m_shadowMap.set_image(0, resolution, glp::TexCube::CF_Z_NEG,
		glp::Tex::IF_RG32F, glp::Tex::PF_RG, glp::Tex::PT_FLOAT, NULL);
	m_shadowMap.set_min_filter(glp::Tex::MNF_LINEAR_MIPMAP_LINEAR);
	m_shadowMap.set_mag_filter(glp::Tex::MGF_LINEAR);
	m_shadowMap.gen_mipmaps();

	m_frbuff.init();
	//glBindFramebuffer(GL_FRAMEBUFFER, 0);

	for (int a = 0; a < 6; ++a)
	{
		m_depthRbuff[a].init();
		m_depthRbuff[a].storage(glp::RenderBuffer::IF_DEPTH_COMPONENT24, resolution, resolution);
	}

	assert(glGetError() == GL_NO_ERROR);
	return true;
}

void PointShadowMap::release()
{
	ShadowMap::release();
}


void PointShadowMap::renderToShadowMap(const PointLight& light,
		const Renderable& ren, const math::Mat4x4f& model, bool clear)
{
	float farPlane = light.range();
	float nearPlane = farPlane/4096.0f;
	math::Mat4x4f proj;
	math::set_projection(proj, math::Vec2f(-nearPlane),
		math::Vec2f(nearPlane), nearPlane, farPlane);
	math::Mat4x4f invView;
	math::Mat4x4f trans;
	math::set_translation(trans, -light.position());

	int oldViewport[4];
	glGetIntegerv(GL_VIEWPORT, oldViewport);
	glViewport(0, 0, m_resolution, m_resolution);

	glp::Device::bind_program(m_shadowProg);

	m_shadowProg.uniform_vec3("lightPos", light.position().m);
	m_shadowProg.uniform_mat4x4("proj", proj.m, true);
	m_shadowProg.uniform_mat4x4("model", model.m, true);

	glp::Device::bind_fbuff(m_frbuff);
	glEnable(GL_DEPTH_TEST);

	invView = trans;
	m_shadowProg.uniform_mat4x4("modelView", (invView*model).m, true);
	m_frbuff.attach_rbuffer(m_depthRbuff[0], glp::FrameBuffer::ATT_DEPTH);
	m_frbuff.attach_tex_face(m_shadowMap, glp::TexCube::CF_Z_POS, 0);
	if (clear) glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	ren.render(false);

	invView = math::Mat4x4f(
		0.0f, 0.0f,-1.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f)*trans;
	m_shadowProg.uniform_mat4x4("modelView", (invView*model).m, true);
	m_frbuff.attach_rbuffer(m_depthRbuff[1], glp::FrameBuffer::ATT_DEPTH);
	m_frbuff.attach_tex_face(m_shadowMap, glp::TexCube::CF_X_POS, 0);
	if (clear) glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	ren.render(false);

	invView = math::Mat4x4f(
		-1.0f, 0.0f, 0.0f, 0.0f,
		 0.0f, 1.0f, 0.0f, 0.0f,
		 0.0f, 0.0f,-1.0f, 0.0f,
		 0.0f, 0.0f, 0.0f, 1.0f)*trans;
	m_shadowProg.uniform_mat4x4("modelView", (invView*model).m, true);
	m_frbuff.attach_rbuffer(m_depthRbuff[2], glp::FrameBuffer::ATT_DEPTH);
	m_frbuff.attach_tex_face(m_shadowMap, glp::TexCube::CF_Z_NEG, 0);
	if (clear) glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	ren.render(false);

	invView = math::Mat4x4f(
		 0.0f, 0.0f, 1.0f, 0.0f,
		 0.0f, 1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f, 0.0f,
		 0.0f, 0.0f, 0.0f, 1.0f)*trans;
	m_shadowProg.uniform_mat4x4("modelView", (invView*model).m, true);
	m_frbuff.attach_rbuffer(m_depthRbuff[3], glp::FrameBuffer::ATT_DEPTH);
	m_frbuff.attach_tex_face(m_shadowMap, glp::TexCube::CF_X_NEG, 0);
	if (clear) glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	ren.render(false);

	invView = math::Mat4x4f(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f,-1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f)*trans;
	m_shadowProg.uniform_mat4x4("modelView", (invView*model).m, true);
	m_frbuff.attach_rbuffer(m_depthRbuff[4], glp::FrameBuffer::ATT_DEPTH);
	m_frbuff.attach_tex_face(m_shadowMap, glp::TexCube::CF_Y_POS, 0);
	if (clear) glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	ren.render(false);

	invView = math::Mat4x4f(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f,-1.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f)*trans;
	m_shadowProg.uniform_mat4x4("modelView", (invView*model).m, true);
	m_frbuff.attach_rbuffer(m_depthRbuff[5], glp::FrameBuffer::ATT_DEPTH);
	m_frbuff.attach_tex_face(m_shadowMap, glp::TexCube::CF_Y_NEG, 0);
	if (clear) glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	ren.render(false);

	glDisable(GL_DEPTH_TEST);
	glp::Device::unbind_fbuff(m_frbuff);

	glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
	assert(glGetError() == GL_NO_ERROR);
}



bool SpotShadowMap::init(int resolution)
{
	if (!ShadowMap::init(resolution))
		return false;

	m_shadowMap.init();
	m_shadowMap.set_image(0, resolution, resolution,
		glp::Tex::IF_RG32F, glp::Tex::PF_RG, glp::Tex::PT_FLOAT, NULL);
	m_shadowMap.set_wrapST(glp::Tex::WM_CLAMP_TO_EDGE);
	m_shadowMap.set_min_filter(glp::Tex::MNF_LINEAR_MIPMAP_LINEAR);
	m_shadowMap.set_mag_filter(glp::Tex::MGF_LINEAR);
	m_shadowMap.gen_mipmaps();

	m_frbuff.init();
	//glBindFramebuffer(GL_FRAMEBUFFER, 0);

	m_depthRbuff.init();
	m_depthRbuff.storage(glp::RenderBuffer::IF_DEPTH_COMPONENT24, resolution, resolution);

	assert(glGetError() == GL_NO_ERROR);
	return true;
}

void SpotShadowMap::release()
{
	m_depthRbuff.release();
	m_frbuff.release();
	ShadowMap::release();
}

void SpotShadowMap::renderToShadowMap(const SpotLight& light,
		const Renderable& ren, const math::Mat4x4f& model, bool clear)
{
	float farPlane = light.range();
	float nearPlane = farPlane/4096.0f;
	math::Mat4x4f proj;
	math::set_projection(proj, math::Vec2f(-nearPlane*light.fov()),
		math::Vec2f(nearPlane*light.fov()), nearPlane, farPlane);

	math::Mat4x4f invView(math::Mat4x4f::I);
	math::Vec3f v0 = light.direction();
	math::Vec3f v1, v2;
	v0.coord_system(v1, v2);
	for (int a = 0; a < 3; ++a)
	{
		invView[0][a] = v2[a];
		invView[1][a] = v1[a];
		invView[2][a] = v0[a];
	}

	math::Mat4x4f trans;
	math::set_translation(trans, -light.position());
	invView *= trans;

	int oldViewport[4];
	glGetIntegerv(GL_VIEWPORT, oldViewport);
	glViewport(0, 0, m_resolution, m_resolution);

	glp::Device::bind_program(m_shadowProg);

	m_shadowProg.uniform_vec3("lightPos", light.position().m);
	m_shadowProg.uniform_mat4x4("proj", proj.m, true);
	m_shadowProg.uniform_mat4x4("model", model.m, true);
	m_shadowProg.uniform_mat4x4("modelView", (invView*model).m, true);

	glp::Device::bind_fbuff(m_frbuff);
	glEnable(GL_DEPTH_TEST);

	m_frbuff.attach_rbuffer(m_depthRbuff, glp::FrameBuffer::ATT_DEPTH);
	m_frbuff.attach_tex_2d(m_shadowMap, 0);
	if (clear) glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	ren.render(false);

	glDisable(GL_DEPTH_TEST);
	glp::Device::unbind_fbuff(m_frbuff);

	glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
	assert(glGetError() == GL_NO_ERROR);
}


void SpotShadowMap::getLightViewProj(const SpotLight& light, math::Mat4x4f& mat)
{
	float farPlane = light.range();
	float nearPlane = farPlane/4096.0f;
	math::Mat4x4f proj;
	math::set_projection(proj, math::Vec2f(-nearPlane*light.fov()),
		math::Vec2f(nearPlane*light.fov()), nearPlane, farPlane);

	math::Mat4x4f invView(math::Mat4x4f::I);
	math::Vec3f v0 = light.direction();
	math::Vec3f v1, v2;
	v0.coord_system(v1, v2);
	for (int a = 0; a < 3; ++a)
	{
		invView[0][a] = v2[a];
		invView[1][a] = v1[a];
		invView[2][a] = v0[a];
	}

	math::Mat4x4f trans;
	math::set_translation(trans, -light.position());
	invView *= trans;

	mat = proj*invView;
}

#include "renderable.h"
#include "glplusx.h"
#include "glplusx_obj.h"
#include "glplusx_tan.h"
#include <assert.h>


bool Renderable::load_plane(float x, float z, float h, float tu, float tv)
{
	stx::vector<math::Vec3f> P;
	stx::vector<math::Vec2f> T;
	stx::vector<math::Vec3f> N;
	stx::vector<glpx::FaceIndexes*> INDS;

	if (!glpx::generate_plane(x, z, h, tu, tv, P, T, N, INDS, true))
		return false;

	if (!generate_geometry(P, T, N, INDS, true))
		return false;

	for (size_t a = 0; a < INDS.size(); ++a)
		delete INDS[a];
	INDS.clear();

	fill_buffers();
	return true;
}

bool Renderable::load_bar(float x, float y, float z)
{
	stx::vector<math::Vec3f> P;
	stx::vector<math::Vec2f> T;
	stx::vector<math::Vec3f> N;
	stx::vector<glpx::FaceIndexes*> INDS;

	if (!glpx::generate_box(x, y, z, P, T, N, INDS, true))
		return false;

	if (!generate_geometry(P, T, N, INDS, true))
		return false;

	for (size_t a = 0; a < INDS.size(); ++a)
		delete INDS[a];
	INDS.clear();

	fill_buffers();
	return true;
}

bool Renderable::load_obj(const wchar_t* fileName, bool swapZ, bool gen_tangent)
{
	stx::vector<math::Vec3f> P;
	stx::vector<math::Vec2f> T;
	stx::vector<math::Vec3f> N;
	stx::vector<glpx::FaceIndexes*> INDS;

	if (!glpx::read_obj_file(fileName, P, T, N, INDS, true))
		return false;
	//if (!glpx::generate_box(0.5f, 0.5f, 0.5f, P, T, N, INDS, true))
	//	return false;
	//if (!glpx::generate_cylinder(0.5f, 0.5f, 64, 1, P, T, N, INDS, true))
	//	return false;

	if (swapZ)
	{
		for (size_t a = 0; a < P.size(); ++a)
			P[a].z = -P[a].z;
		for (size_t a = 0; a < N.size(); ++a)
			N[a].z = -N[a].z;
	}

	if (!generate_geometry(P, T, N, INDS, gen_tangent))
		return false;

	for (size_t a = 0; a < INDS.size(); ++a)
		delete INDS[a];
	INDS.clear();

	if (m_geometry.v.empty())
		return false;

	fill_buffers();
	return true;
}

bool Renderable::addTextures(const char* name, const wchar_t* texDiff,
		const wchar_t* texNormal, const wchar_t* texHeight)
{
	TexSet* ts = new TexSet();

	if (texDiff != nullptr) {
		ts->m_texDiff.init();
		if (!glpx::LoadTex2D_RGBA(ts->m_texDiff, texDiff))
			return false;
	}

	if (texNormal != nullptr) {
		ts->m_texNormal.init();
		if (!glpx::LoadTex2D_RGBA(ts->m_texNormal, texNormal))
			return false;
	}

	if (texHeight != nullptr) {
		ts->m_texHeight.init();
		if (!glpx::LoadTex2D_RGBA(ts->m_texHeight, texHeight))
			return false;
	}

	m_textures.insert(std::make_pair(std::string(name), ts));
	return true;
}

void Renderable::release()
{
}

void Renderable::render(bool useTextures) const
{
	glp::Device::bind_vertex_array(m_varray);

	for (size_t a = 0; a < m_geometry.m.size(); ++a)
	{
		const TexSet* ts = nullptr;
		/*if (useTextures)
		{
			std::map<std::string, TexSet*>::const_iterator it = m_textures.find(m_geometry.m[a]->name);
			if (it != m_textures.end()) ts = it->second;
		}*/
		ts = m_textures.begin()->second;

		if (ts)
		{
			glp::Device::bind_tex(ts->m_texDiff, 0);
			glp::Device::bind_tex(ts->m_texNormal, 1);
			glp::Device::bind_tex(ts->m_texHeight, 2);
		}

		if (m_geometry.m[a]->t.empty())
			continue;

		glDrawElements(GL_TRIANGLES,
			3*m_geometry.m[a]->t.size(),
			GL_UNSIGNED_INT,
			&m_geometry.m[a]->t.front());

		if (ts)
		{
			glp::Device::unbind_tex(ts->m_texHeight, 2);
			glp::Device::unbind_tex(ts->m_texNormal, 1);
			glp::Device::unbind_tex(ts->m_texDiff, 0);
		}
	}

	glp::Device::unbind_vertex_array(m_varray);
	assert(glGetError() == GL_NO_ERROR);
}


bool Renderable::generate_geometry(
		const glpx::ArrayVec3f& positions,
		const glpx::ArrayVec2f& tex_coords,
		const glpx::ArrayVec3f& normals,
		const stx::vector<glpx::FaceIndexes*>& indexes,
		bool gen_tangent)
{
	stx::vector<glpx::MeshIndexes> meshes;
	stx::vector<glpx::VertexIndexes> vertexes;
	glpx::generate_meshes(meshes, vertexes, indexes);

	m_geometry.v.resize(vertexes.size());
	for (size_t a = 0; a < vertexes.size(); ++a)
	{
		m_geometry.v[a].point = positions[vertexes[a].ix_position];
		m_geometry.v[a].texcoord = tex_coords[vertexes[a].ix_tex_coord];
		m_geometry.v[a].normal = normals[vertexes[a].ix_normal];
	}

	m_geometry.m.resize(meshes.size());
	for (size_t m = 0; m < meshes.size(); ++m)
	{
		m_geometry.m[m] = new Mesh(indexes[m]->material_name.c_str());

		m_geometry.m[m]->t.resize(meshes[m].triangles.size()/3);
		for (size_t a = 0; a < m_geometry.m[m]->t.size(); ++a)
		{
			m_geometry.m[m]->t[a].v[0] = meshes[m].triangles[3*a + 0];
			m_geometry.m[m]->t[a].v[1] = meshes[m].triangles[3*a + 1];
			m_geometry.m[m]->t[a].v[2] = meshes[m].triangles[3*a + 2];
		}

		m_geometry.m[m]->q.resize(meshes[m].quads.size()/4);
		for (size_t a = 0; a < m_geometry.m[m]->q.size(); ++a)
		{
			m_geometry.m[m]->q[a].v[0] = meshes[m].quads[4*a + 0];
			m_geometry.m[m]->q[a].v[1] = meshes[m].quads[4*a + 1];
			m_geometry.m[m]->q[a].v[2] = meshes[m].quads[4*a + 2];
			m_geometry.m[m]->q[a].v[3] = meshes[m].quads[4*a + 3];
		}
	}

	if (gen_tangent) 
	{
		for (size_t a = 0; a < m_geometry.m.size(); ++a)
		{
			if (!glpx::generate_tangents(
				m_geometry.v.size(), sizeof(Vertex),
				m_geometry.v.front().point.m,
				m_geometry.v.front().texcoord.m,
				m_geometry.v.front().normal.m,
				m_geometry.v.front().tgtU.m,
				m_geometry.v.front().tgtV.m,
				m_geometry.m[a]->t.size(),
				sizeof(Triangle),
				m_geometry.m[a]->t.front().v, false))
				return false;
		}
	}

	return true;
}

void Renderable::fill_buffers()
{
	m_vbuff.init();
	m_vbuff.buffer_data(m_geometry.v.size()*sizeof(Vertex),
		glp::Buffer::UM_STATIC_DRAW, &m_geometry.v.front());

	m_varray.init();

	glp::Device::bind_vertex_array(m_varray);

	glp::Device::bind_buffer(m_vbuff);
	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scene->m_ib);

	glEnableVertexAttribArray(ATTR_LOC_POINT);
	glEnableVertexAttribArray(ATTR_LOC_COORD);
	glEnableVertexAttribArray(ATTR_LOC_NORMAL);
	glEnableVertexAttribArray(ATTR_LOC_TGT_U);
	glEnableVertexAttribArray(ATTR_LOC_TGT_V);
	glVertexAttribPointer(ATTR_LOC_POINT, 3, GL_FLOAT, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, point));
	glVertexAttribPointer(ATTR_LOC_COORD, 2, GL_FLOAT, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, texcoord));
	glVertexAttribPointer(ATTR_LOC_NORMAL, 3, GL_FLOAT, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
	glVertexAttribPointer(ATTR_LOC_TGT_U, 3, GL_FLOAT, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, tgtU));
	glVertexAttribPointer(ATTR_LOC_TGT_V, 3, GL_FLOAT, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, tgtV));

	glp::Device::unbind_vertex_array(m_varray);
	glp::Device::unbind_buffer(m_vbuff);

	assert(glGetError() == GL_NO_ERROR);
}

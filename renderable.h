#ifndef renderableH
#define renderableH

#include "glplus.h"
#include "glplusx.h"
#include "glplusx_obj.h"
#include <map>
#include <string>


struct Vertex
{
	Vertex(): point(0.0f), texcoord(0.0f),
		normal(0.0f), tgtU(0.0f), tgtV(0.0f) {}
	math::Vec3f point;
	math::Vec2f texcoord;
	math::Vec3f normal;
	math::Vec3f tgtU;
	math::Vec3f tgtV;
};

struct Triangle
{
	uint v[3];
};

struct Quad
{
	uint v[4];
};

struct Mesh
{
	Mesh(const char* name): material_name(name) {}
	stx::string material_name;
	stx::vector<Triangle> t;
	stx::vector<Quad> q;
};

struct GeomData
{
	stx::vector<Vertex> v;
	stx::vector<Mesh*> m;
};


class Renderable
{
public:
	bool load_plane(float x, float z, float h, float tu, float tv);
	bool load_grid(float x, float z, float h, uint grid_x, uint grid_z, float tu, float tv);
	bool load_box(float x, float y, float z);
	bool load_obj(const wchar_t* fileName, bool swapZ, bool gen_tangent);

	void release();

	bool addTextures(const char* name, const wchar_t* texDiff,
		const wchar_t* texNormal, const wchar_t* texHeight);

	void render(bool useTextures) const;
	const GeomData& getGeometry() const {return m_geometry;}

	static const GLuint ATTR_LOC_POINT  = 0;
	static const GLuint ATTR_LOC_COORD  = 1;
	static const GLuint ATTR_LOC_NORMAL = 2;
	static const GLuint ATTR_LOC_TGT_U  = 3;
	static const GLuint ATTR_LOC_TGT_V  = 4;

private:
	struct TexSet
	{
		glp::Tex2D m_texDiff;
		glp::Tex2D m_texNormal;
		glp::Tex2D m_texHeight;
	};

	bool generate_geometry(
		const glpx::ArrayVec3f& positions,
		const glpx::ArrayVec2f& tex_coords,
		const glpx::ArrayVec3f& normals,
		const stx::vector<glpx::FaceIndexes*>& indexes,
		bool gen_tangent); // added as it was failing for pool.obj
	void fill_buffers();

	GeomData m_geometry;
	glp::VertexBuffer m_vbuff;
	glp::VertexArray m_varray;
	std::map<std::string, TexSet*> m_textures;
};


#endif

#ifndef lightH
#define lightH

#include "mathx.h"
#include "glplus.h"
#include "renderable.h"


class Light;
class SceneLight;
class PointLight;
class SpotLight;
class DirectionalLight;

class ShadowMap;
class PointShadowMap;
class SpotShadowMap;
class DirectionalShadowMap;


class Light
{
public:
	Light() {}
	Light(const math::Vec3f& ambient, const math::Vec3f& intensity):
			m_ambient(ambient), m_intensity(intensity) {}

	const math::Vec3f& ambient() const {return m_ambient;}
	math::Vec3f& ambient() {return m_ambient;}

	const math::Vec3f& intensity() const {return m_intensity;}
	math::Vec3f& intensity() {return m_intensity;}

protected:
	math::Vec3f m_ambient;
	math::Vec3f m_intensity;
};


class SceneLight: public Light
{
public:
	SceneLight() {}
	SceneLight(const math::Vec3f& ambient, const math::Vec3f& intensity,
		const math::Vec3f& position, float range, float linearAtt):
		Light(ambient, intensity), m_position(position), m_range(range),
		m_linearAtt(linearAtt) {}

	const math::Vec3f& position() const {return m_position;}
	math::Vec3f& position() {return m_position;}

	const float& range() const {return m_range;}
	float& range() {return m_range;}

	const float& linearAtt() const {return m_linearAtt;}
	float& linearAtt() {return m_linearAtt;}

protected:
	math::Vec3f m_position;
	float m_range;
	float m_linearAtt;
};


class PointLight: public SceneLight
{
public:
	PointLight() {}
	PointLight(const math::Vec3f& ambient, const math::Vec3f& intensity,
		const math::Vec3f& position, float range, float linearAtt):
		SceneLight(ambient, intensity, position, range, linearAtt) {}

private:
};


class SpotLight: public SceneLight
{
public:
	SpotLight() {}
	SpotLight(const math::Vec3f& ambient, const math::Vec3f& intensity,
		const math::Vec3f& position, float range, float linearAtt,
		const math::Vec3f& direction, float fov, float directionalAtt):
		SceneLight(ambient, intensity, position, range, linearAtt),
		m_direction(direction), m_fov(fov), m_directionalAtt(directionalAtt) {}

	const math::Vec3f& direction() const {return m_direction;}
	math::Vec3f& direction() {return m_direction;}

	const float& fov() const {return m_fov;}
	float& fov() {return m_fov;}

	const float& directionalAtt() const {return m_directionalAtt;}
	float& directionalAtt() {return m_directionalAtt;}

private:
	math::Vec3f m_direction;
	float m_fov;
	float m_directionalAtt;
};


class DirectionalLight: public Light
{
public:
	DirectionalLight() {}

private:
	math::Vec3f m_direction;
};





class ShadowMap
{
public:
	ShadowMap(): m_resolution(0) {}
	~ShadowMap() {}

	bool init(int resolution);
	void release();

protected:
	glp::Program m_shadowProg;
	int m_resolution;
};


class PointShadowMap: public ShadowMap
{
public:
	PointShadowMap() {}
	~PointShadowMap() {}

	bool init(int resolution);
	void release();

	glp::TexCube& getShadowMap() {return m_shadowMap;}
	void renderToShadowMap(const PointLight& light,
		const Renderable& ren, const math::Mat4x4f& model, bool clear);

private:
	glp::FrameBuffer m_frbuff;
	glp::RenderBuffer m_depthRbuff[6];
	glp::TexCube m_shadowMap;
};


class SpotShadowMap: public ShadowMap
{
public:
	SpotShadowMap() {}
	~SpotShadowMap() {}

	bool init(int resolution);
	void release();

	glp::Tex2D& getShadowMap() {return m_shadowMap;}
	void renderToShadowMap(const SpotLight& light,
		const Renderable& ren, const math::Mat4x4f& model, bool clear);
	static void getLightViewProj(const SpotLight& light,
		math::Mat4x4f& mat);

private:
	glp::FrameBuffer m_frbuff;
	glp::RenderBuffer m_depthRbuff;
	glp::Tex2D m_shadowMap;
};


class DirectionalShadowMap: public ShadowMap
{
public:
private:
};


#endif

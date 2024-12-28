#include "GraphicsHandler.h"

#define CAMERA_DEFAULT_NEAR_CLIP 0.01f
#define CAMERA_DEFAULT_FAR_CLIP 1000.f

class Camera {
public:
	Camera();
	Camera(glm::vec3 p, glm::vec3 f, float fov, float ar);
	~Camera() = default;

	const glm::mat4& getVP() const {return vp;}
	const glm::vec3& getForward() const {return forward;}
	const glm::vec3& getRight() const {return right;}
	const glm::vec3& getUp() const {return up;}
	const glm::vec3& getPos() const {return position;}
	float getFOVY() const {return fovy;}

	void setPos(glm::vec3 p);
	/* 
	 * normalizes provided vector...possible efficiency loss, could provide one that
	 * expects a pre-normalized vector
	 */
	void setForward(glm::vec3 f);
	void setFOVY(float f);

private:
	glm::vec3 position, forward, right, up;
	glm::mat4 view, projection, vp;
	float fovy, aspectratio, nearclip, farclip;

	void updateView();
	void updateProj();
};

#define LIGHT_SHADOW_MAP_FORMAT VK_FORMAT_D32_SFLOAT // consider more efficient formats...

class Light {
public:
	Light();
	Light(glm::vec3 p, glm::vec3 c);
	~Light();

	const ImageInfo& getShadowMap() const {return shadowmap;}
	const PipelineInfo& getSMPipeline() const {return smpipeline;}
	virtual const void* getPCData() const = 0;

	// void setSMExtent(
	void setSMPipeline(PipelineInfo p) {smpipeline = p;}

protected:
	glm::vec3 position;
	ImageInfo shadowmap; // if shadowmap.image == VK_NULL_HANDLE, presumed to not do shadowmapping

	static constexpr VkSamplerCreateInfo defaultshadowsamplerci {
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		nullptr,
		0,
		VK_FILTER_LINEAR, VK_FILTER_LINEAR,
		VK_SAMPLER_MIPMAP_MODE_NEAREST,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		0.,
		VK_FALSE, 0.,
		// VK_TRUE, VK_COMPARE_OP_LESS,
		VK_FALSE, VK_COMPARE_OP_LESS,
		0., 0., 
		VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
		VK_FALSE
	};

private:
	glm::vec3 color; // intensity baked-in to color, this is not normalized in the shader
	PipelineInfo smpipeline;
};

class DirectionalLight : public Light {
public:
	DirectionalLight() : Light() {}
	DirectionalLight(glm::vec3 p, glm::vec3 f, glm::vec3 c);
	~DirectionalLight() = default;

	const glm::mat4& getVP() const {return vp;}
	const void* getPCData() const {return &vp;}

protected:
	glm::vec3 forward;
	glm::mat4 view, projection, vp;

private:
	void updateView();
	virtual void updateProj() = 0;
};

class SunLight : public DirectionalLight {
public:
	SunLight() : DirectionalLight() {}
	SunLight(glm::vec3 p, glm::vec3 f, glm::vec3 c);
	~SunLight() = default;
private:
	void updateProj();
};

/*
class PointLight : public Light {

};
*/

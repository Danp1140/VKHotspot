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
#define LIGHT_SHADOW_AABB_FUDGE 1.f

typedef struct LightInitInfo {
	glm::vec3 p = glm::vec3(1), c = glm::vec3(1);
	VkExtent2D sme = {0, 0};
} LightInitInfo;

class Light {
public:
	Light() : Light((LightInitInfo){}) {}
	Light(glm::vec3 p, glm::vec3 c);
	Light(LightInitInfo&& i);
	~Light();

	Light& operator=(const Light& rhs) = delete;
	Light& operator=(Light&& rhs);

	friend void swap(Light& lhs, Light& rhs);

	virtual void addVecToFocus(const glm::vec3& v);

	virtual void setPos(glm::vec3 p) {position = p;}
	void setCol(glm::vec3 c) {color = c;}

	const glm::vec3& getPos() const {return position;}
	const glm::vec3& getCol() const {return color;}
	const ImageInfo& getShadowMap() const {return shadowmap;}
	const PipelineInfo& getSMPipeline() const {return smpipeline;}

	void setSMPipeline(PipelineInfo p) {smpipeline = p;}

	static const glm::mat4 constexpr smadjmat = glm::mat4(
		0.5f, 0.f,  0.f, 0.f,
		0.f,  0.5f, 0.f, 0.f,
		0.f,  0.f,  1.f, 0.f,
		0.5f, 0.5f, 0.f, 1.f
	);

protected:
	glm::vec3 position;
	ImageInfo shadowmap; // if shadowmap.image == VK_NULL_HANDLE, presumed to not do shadowmapping
	glm::vec3 focus[2];

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
	PipelineInfo smpipeline; // redundant with storage in Scene's RP's RS, here for convenience
	// TODO: consider making a reference ^^^
};

typedef enum DirectionalLightType {
	DIRECTIONAL_LIGHT_TYPE_ORTHO,
	DIRECTIONAL_LIGHT_TYPE_PERSP
} DirectionalLightType;

typedef struct DirectionalLightInitInfo {
	LightInitInfo super = {};
	DirectionalLightType t = DIRECTIONAL_LIGHT_TYPE_ORTHO;
	glm::vec3 f = glm::vec3(-1);
} DirectionalLightInitInfo;

class DirectionalLight : public Light {
public:
	DirectionalLight() : DirectionalLight((DirectionalLightInitInfo){}) {}
	DirectionalLight(DirectionalLightInitInfo&& i);
	DirectionalLight(glm::vec3 p, glm::vec3 f, glm::vec3 c);
	~DirectionalLight() = default;

	DirectionalLight& operator=(const DirectionalLight& rhs) = delete;
	DirectionalLight& operator=(DirectionalLight&& rhs);

	friend void swap(DirectionalLight& lhs, DirectionalLight& rhs);

	void addVecToFocus(const glm::vec3& v);

	void setPos(glm::vec3 p);
	void setForward(glm::vec3 f);

	const glm::mat4& getVP() const {return vp;}

protected:
	DirectionalLightType type;
	glm::vec3 forward;
	glm::mat4 view, projection, vp;

private:
	void updateView();
	virtual void updateProj();
};

/*
class PointLight : public Light {

};
*/

#include "GraphicsHandler.h"

#define CAMERA_DEFAULT_NEAR_CLIP 1.f
#define CAMERA_DEFAULT_FAR_CLIP 1000.f

class Camera {
public:
	Camera();
	Camera(glm::vec3 p, glm::vec3 f, float fov, float ar);
	~Camera() = default;

	const glm::mat4& getVP() const {return vp;}
	const glm::mat4& getView() const {return view;}
	const glm::mat4& getProj() const {return projection;}
	const glm::vec3& getForward() const {return forward;}
	const glm::vec3& getRight() const {return right;}
	const glm::vec3& getUp() const {return up;}
	const glm::vec3& getPos() const {return position;}
	float getFOVY() const {return fovy;}
	float getNearClip() const {return nearclip;}

	void setPos(glm::vec3 p);
	/* 
	 * normalizes provided vector...possible efficiency loss, could provide one that
	 * expects a pre-normalized vector
	 */
	void setForward(glm::vec3 f);
	void setFOVY(float f);
	void setFarClip(float f);
	void setVP(const glm::mat4& m) {vp = m;}

private:
	glm::vec3 position, forward, right, up;
	glm::mat4 view, projection, vp;
	float fovy, aspectratio, nearclip, farclip;

	void updateView();
	void updateProj();
};

#define LIGHT_SHADOW_MAP_FORMAT VK_FORMAT_D32_SFLOAT // consider more efficient formats...
#define LIGHT_SHADOW_AABB_FUDGE 0.f

typedef struct LightInitInfo {
	glm::vec3 p = glm::vec3(1), c = glm::vec3(1);
	VkExtent2D sme = {0, 0};
} LightInitInfo;

class Light {
public:
	Light() : Light((LightInitInfo){}) {}
	Light(glm::vec3 p, glm::vec3 c);
	Light(const LightInitInfo& i);
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

	static glm::vec3 apply(glm::mat4 A, glm::vec3 v);
	static glm::vec3 applyHomo(glm::mat4 A, glm::vec3 v);
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

typedef enum LightSMType {
	LIGHT_SM_TYPE_UNIFORM,
	LIGHT_SM_TYPE_PERSPECTIVE,
	LIGHT_SM_TYPE_LISP,
	LIGHT_SM_TYPE_TRAPEZOID
} LightSMType;

typedef struct DirectionalLightInitInfo {
	LightInitInfo super = {};
	DirectionalLightType t = DIRECTIONAL_LIGHT_TYPE_ORTHO;
	glm::vec3 f = glm::vec3(-1);
	LightSMType sm_t = LIGHT_SM_TYPE_UNIFORM;
	void* sm_dat = nullptr;
} DirectionalLightInitInfo;

class DirectionalLight : public Light {
public:
	DirectionalLight() : DirectionalLight((DirectionalLightInitInfo){}) {}
	DirectionalLight(const DirectionalLightInitInfo& i);
	DirectionalLight(glm::vec3 p, glm::vec3 f, glm::vec3 c);
	~DirectionalLight() = default;

	DirectionalLight& operator=(const DirectionalLight& rhs) = delete;
	DirectionalLight& operator=(DirectionalLight&& rhs);

	friend void swap(DirectionalLight& lhs, DirectionalLight& rhs);

	void addVecToFocus(const glm::vec3& v);

	// TODO: clean up deprecated access functions
	void setPos(glm::vec3 p);
	void setForward(glm::vec3 f);
	void setLSP(const glm::mat4& m) {lsp = m;}

	const glm::mat4& getVP() const {return vp;}
	const glm::mat4& getLSP() const {return lsp;}
	const glm::vec3& getForward() const {return forward;}
	void updateView();
	void updateProj();

protected:
	DirectionalLightType type;
	LightSMType sm_type;
	void* sm_data;
	glm::vec3 forward;
	glm::mat4 view, projection, vp, lsp;

private:
};

/*
class PointLight : public Light {

};
*/

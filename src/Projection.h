#include "GraphicsHandler.h"

typedef enum ProjectionType {
	PROJECTION_TYPE_ORTHO,
	PROJECTION_TYPE_PERSP
} DirectionalLightType;

class ProjectionBase {
public:
	const glm::mat4& getVP() const {return vp;}
	const glm::mat4& getView() const {return view;}
	const glm::mat4& getProj() const {return projection;}

	static glm::vec3 apply(glm::mat4 A, glm::vec3 v);
	static glm::vec3 applyHomo(glm::mat4 A, glm::vec3 v);

protected:
	glm::mat4 view, projection, vp;

	virtual void updateView() = 0;
	virtual void updateProj() = 0;
};

class PositionalProjectionBase {
public:
	PositionalProjectionBase() : position(0) {}
	PositionalProjectionBase(const glm::vec3& p) : position(p) {}

	const glm::vec3& getPos() const {return position;}

	void setPos(const glm::vec3& p);

protected:
	glm::vec3 position;
};

class DirectionalProjectionBase {
public:
	DirectionalProjectionBase() : forward(glm::vec3(0, 0, -1)) {}
	DirectionalProjectionBase(const glm::vec3& f) : forward(f) {}

	const glm::vec3& getForward() const {return forward;}

	void setForward(const glm::vec3& f);

protected:
	glm::vec3 forward;
};

#define CAMERA_DEFAULT_NEAR_CLIP 1.f
#define CAMERA_DEFAULT_FAR_CLIP 100.f

class Camera : public ProjectionBase, public PositionalProjectionBase, public DirectionalProjectionBase {
public:
	Camera();
	Camera(glm::vec3 p, glm::vec3 f, float fov, float ar);
	~Camera() = default;
	
	float getFOVY() const {return fovy;}
	float getNearClip() const {return nearclip;}
	float getFarClip() const {return farclip;}

	void setFOVY(float f);

private:
	float fovy, aspectratio, nearclip, farclip;

	void updateView();
	void updateProj();
};

#define LIGHT_SHADOW_MAP_FORMAT VK_FORMAT_D32_SFLOAT // consider more efficient formats...
#define LIGHT_SHADOW_AABB_FUDGE 0.f

// simple uniform vs cascade is manipulated by various focusses and numbers of sm data
class LightSMData : public ProjectionBase {
public:
	void addVecToFocus(const glm::vec3& v);

	void setView(const glm::mat4& v) {view = v;}
	void setProj(const glm::mat4& p) {projection = p;}

	void updateView() {vp = projection * view;}
	void updateProj() {vp = projection * view;}

private:
	// offset for location in atlas if being used
	VkExtent2D extent = {0, 0}, offset = {0, 0};
	ImageInfo* sm = nullptr;
	glm::vec3 focus[2] = {glm::vec3(std::numeric_limits<float>::infinity()), -glm::vec3(std::numeric_limits<float>::infinity())};
};

typedef struct LightInitInfo {
	glm::vec3 c = glm::vec3(1);
} LightInitInfo;

class Light {
public:
	Light() : Light((LightInitInfo){}) {}
	Light(const LightInitInfo& i);

	Light& operator=(const Light& rhs) = delete;
	Light& operator=(Light&& rhs);

	friend void swap(Light& lhs, Light& rhs);

	const glm::vec3& getCol() const {return color;}
	const std::vector<LightSMData>& getSMData() const {return sm_data;}

	void setCol(glm::vec3 c) {color = c;}

	void addVecToFocus(const glm::vec3& v, size_t i);

	static const glm::mat4 constexpr smadjmat = glm::mat4(
		0.5f, 0.f,  0.f, 0.f,
		0.f,  0.5f, 0.f, 0.f,
		0.f,  0.f,  1.f, 0.f,
		0.5f, 0.5f, 0.f, 1.f
	);

protected:
	std::vector<LightSMData> sm_data;

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
};

typedef struct DirectionalLightInitInfo {
	LightInitInfo super = {};
} DirectionalLightInitInfo;

class DirectionalLight : public Light, public DirectionalProjectionBase {
public:
	DirectionalLight() : DirectionalLight((DirectionalLightInitInfo){}) {}
	DirectionalLight(const DirectionalLightInitInfo& i);
	~DirectionalLight() = default;

	DirectionalLight& operator=(const DirectionalLight& rhs) = delete;
	DirectionalLight& operator=(DirectionalLight&& rhs);

	friend void swap(DirectionalLight& lhs, DirectionalLight& rhs);

	void updateSMDatum(size_t sm_i);

private:
};

class SpotLight : public Light, public PositionalProjectionBase, public DirectionalProjectionBase {

};

class PointLight : public Light, public PositionalProjectionBase {

};

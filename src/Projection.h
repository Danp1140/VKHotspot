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

	virtual void updateView() = 0;
	virtual void updateProj() = 0;

protected:
	glm::mat4 view, projection, vp;
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

typedef struct DirectionalProjectionBaseInitInfo {
	glm::vec3 f;
} DirectionalProjectionBaseInitInfo;

class DirectionalProjectionBase {
public:
	DirectionalProjectionBase() : forward(glm::vec3(0, 0, -1)) {}
	DirectionalProjectionBase(const glm::vec3& f) : forward(f) {}
	DirectionalProjectionBase(const DirectionalProjectionBaseInitInfo& ii) : forward(ii.f) {}

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

	void updateView();
	void updateProj();

private:
	float fovy, aspectratio, nearclip, farclip;
};

#define LIGHT_SHADOW_MAP_FORMAT VK_FORMAT_D32_SFLOAT // consider more efficient formats...
#define LIGHT_SHADOW_AABB_FUDGE 0.f

// simple uniform vs cascade is manipulated by various focusses and numbers of sm data
class LightSMData : public ProjectionBase {
public:
	void addVecToFocus(const glm::vec3& v);

	const VkExtent2D& getExtent() const {return extent;}
	const VkExtent2D& getOffset() const {return offset;}
	VkViewport getViewport() const {return {(float)offset.width, (float)offset.height, (float)extent.width, (float)extent.height, 0, 1};}
	VkRect2D getScissor() const {return {{(int32_t)offset.width, (int32_t)offset.height}, extent};}
	glm::vec4 getUVExtOff(const VkExtent2D& sa_ext) const {return glm::vec4(
			(float)extent.width/(float)sa_ext.width,
			(float)extent.height/(float)sa_ext.width,
			(float)offset.width/(float)sa_ext.width,
			(float)offset.height/(float)sa_ext.width);}
	const ImageInfo* getSM() const {return sm;}
	const glm::vec3* getFocus() const {return &focus[0];}

	void setView(const glm::mat4& v) {view = v;}
	void setProj(const glm::mat4& p) {projection = p;}
	void setExtent(const VkExtent2D& e) {extent = e;}
	void setOffset(const VkExtent2D& o) {offset = o;}
	void setSM(ImageInfo* s) {sm = s;}

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

	/*
	Light& operator=(const Light& rhs) = delete;
	Light& operator=(Light&& rhs);
	*/

	friend void swap(Light& lhs, Light& rhs);

	const glm::vec3& getCol() const {return color;}
	const std::vector<LightSMData>& getSMData() const {return sm_data;}

	void addSMData(const LightSMData& d) {sm_data.push_back(d);}

	void setCol(glm::vec3 c) {color = c;}

	void addVecToFocus(const glm::vec3& v, size_t i);

	static const glm::mat4 constexpr smadjmat = glm::mat4(
		0.5f, 0.f,  0.f, 0.f,
		0.f,  0.5f, 0.f, 0.f,
		0.f,  0.f,  1.f, 0.f,
		0.5f, 0.5f, 0.f, 1.f
	);

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

protected:
	std::vector<LightSMData> sm_data;

private:
	glm::vec3 color; // intensity baked-in to color, this is not normalized in the shader
};

typedef struct DirectionalLightInitInfo {
	LightInitInfo super_light = {};
	DirectionalProjectionBaseInitInfo super_directional = {};
} DirectionalLightInitInfo;

class DirectionalLight : public Light, public DirectionalProjectionBase {
public:
	DirectionalLight() : DirectionalLight((DirectionalLightInitInfo){}) {}
	DirectionalLight(const DirectionalLightInitInfo& ii) : 
		Light(ii.super_light), 
		DirectionalProjectionBase(ii.super_directional) {}
	~DirectionalLight() = default;

	/*
	DirectionalLight& operator=(const DirectionalLight& rhs) = delete;
	DirectionalLight& operator=(DirectionalLight&& rhs);
	*/

	friend void swap(DirectionalLight& lhs, DirectionalLight& rhs);

	void updateSMDatum(size_t sm_i);

private:
};

class SpotLight : public Light, public PositionalProjectionBase, public DirectionalProjectionBase {

};

class PointLight : public Light, public PositionalProjectionBase {

};

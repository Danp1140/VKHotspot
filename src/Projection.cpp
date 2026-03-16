#include "Projection.h"

/*
 * Camera
 */

// -- Public --

Camera::Camera() : 
	position(glm::vec3(0, 0, 1)), 
	forward(glm::vec3(0, 0, -1)),
	fovy(glm::quarter_pi<float>()),
	aspectratio(1),
	nearclip(CAMERA_DEFAULT_NEAR_CLIP),
	farclip(CAMERA_DEFAULT_FAR_CLIP) {
	updateView();
	updateProj();
}

Camera::Camera(glm::vec3 p, glm::vec3 f, float fov, float ar) : 
	position(p), 
	forward(f), 
	fovy(fov), 
	aspectratio(ar),
	nearclip(CAMERA_DEFAULT_NEAR_CLIP),
	farclip(CAMERA_DEFAULT_FAR_CLIP) {
	updateView();
	updateProj();
}

void Camera::setPos(glm::vec3 p) {
	position = p;
	updateView();
}

void Camera::setForward(glm::vec3 f) {
	forward = glm::normalize(f);
	updateView();
}

void Camera::setFOVY(float f) {
	fovy = f;
	updateProj();
}

void Camera::setFarClip(float f) {
	farclip = f;
	updateProj();
}

// -- Private --

void Camera::updateView() {
	right = glm::normalize(glm::vec3(-forward.z, 0, forward.x));
	up = glm::cross(right, forward);
	view = glm::lookAt<float>(position, position + forward, glm::vec3(0, 1, 0));
	vp = projection * view;
}

// starting with just a typical perspective cam
void Camera::updateProj() {
	projection = glm::perspective<float>(fovy, aspectratio, nearclip, farclip); 
	// below adjusts for vulkan's lower-left viewport origin
	// we could also flip the viewport in GH's pipeline create function, something to
	// consider later
	projection[1][1] *= -1;
	vp = projection * view;
}

Light::Light(const LightInitInfo& i) :
	position(i.p),
	color(i.c) {
	if (i.sme.width > 0 && i.sme.height > 0) {
		shadowmap.extent = i.sme;
		shadowmap.format = LIGHT_SHADOW_MAP_FORMAT;
		shadowmap.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		shadowmap.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		// ^^^ transitioned to DEPTH_STENCIL_READ_ONLY_OPTIMAL after renderpass
		// TODO: reuse one sampler
		vkCreateSampler(GH::getLD(), &defaultshadowsamplerci, nullptr, &shadowmap.sampler);
		GH::createImage(shadowmap);
	}
	else shadowmap = {};
	focus[0] = glm::vec3(std::numeric_limits<float>::infinity());
	focus[1] = glm::vec3(-std::numeric_limits<float>::infinity());
}

Light::Light(glm::vec3 p, glm::vec3 c) : Light() {
	position = p;
	color = c;
}

Light::~Light() {
	if (shadowmap.image != VK_NULL_HANDLE) {
		vkDestroySampler(GH::getLD(), shadowmap.sampler, nullptr);
		GH::destroyImage(shadowmap);
	}
}

Light& Light::operator=(Light&& rhs) {
	swap(*this, rhs);
	rhs.shadowmap = {};
	return *this;
}

void swap(Light& lhs, Light& rhs) {
	std::swap(lhs.position, rhs.position);
	std::swap(lhs.color, rhs.color);
	std::swap(lhs.shadowmap, rhs.shadowmap);
	std::swap(lhs.smpipeline, rhs.smpipeline);
}

glm::vec3 Light::apply(glm::mat4 A, glm::vec3 v) {
	glm::vec4 u = A * glm::vec4(v.x, v.y, v.z, 1);
	return glm::vec3(u.x, u.y, u.z);
}

glm::vec3 Light::applyHomo(glm::mat4 A, glm::vec3 v) {
	glm::vec4 u = A * glm::vec4(v.x, v.y, v.z, 1);
	return glm::vec3(u.x, u.y, u.z) / u.w;
}

void Light::addVecToFocus(const glm::vec3& v) {
	for (uint8_t i = 0; i < 3; i++) {
		focus[0][i] = std::min(focus[0][i], v[i]);
		focus[1][i] = std::max(focus[1][i], v[i]);
	}
}

DirectionalLight::DirectionalLight(const DirectionalLightInitInfo& i) : 
	Light(i.super), 
	type(i.t),
	forward(i.f),
	sm_type(i.sm_t),
	sm_data(i.sm_dat) {
	updateView();
	updateProj();
}

DirectionalLight::DirectionalLight(glm::vec3 p, glm::vec3 f, glm::vec3 c) : forward(f), Light(p, c) { 
	updateView();
	updateProj();
}

DirectionalLight& DirectionalLight::operator=(DirectionalLight&& rhs) {
	swap(*this, rhs);
	rhs.shadowmap = {};
	return *this;
}

void swap(DirectionalLight& lhs, DirectionalLight& rhs) {
	swap(static_cast<Light&>(lhs), static_cast<Light&>(rhs));
	std::swap(lhs.forward, rhs.forward);
	std::swap(lhs.vp, rhs.vp);
	std::swap(lhs.view, rhs.view);
	std::swap(lhs.projection, rhs.projection);
	std::swap(lhs.sm_type, rhs.sm_type);
	std::swap(lhs.sm_data, rhs.sm_data);
}

void DirectionalLight::addVecToFocus(const glm::vec3& v) {
	Light::addVecToFocus(v);
	updateProj();
}

void DirectionalLight::setPos(glm::vec3 p) {
	position = p;
	updateView();
	updateProj();
}

void DirectionalLight::setForward(glm::vec3 f) {
	forward = f;
	updateView();
	updateProj();
}

void DirectionalLight::updateView() {
	if (sm_type == LIGHT_SM_TYPE_UNIFORM) view = glm::lookAt<float>(position, position + forward, glm::vec3(0, 1, 0));
	else if (sm_type == LIGHT_SM_TYPE_PERSPECTIVE) FatalError("Perspective SM not yet implemented").raise();
	else if (sm_type == LIGHT_SM_TYPE_LISP) {
		Camera* c = (Camera*)sm_data;
		// TODO: clean up math and optimize n
		glm::vec3 c_right = glm::cross(c->getForward(), glm::vec3(0, 1, 0));
		glm::vec3 p_up = -forward;
		glm::vec3 p_fwd = glm::cross(p_up, c_right);
		float n = 10;
		glm::vec3 p_pos = c->getPos() - n * p_fwd; 
		// glm::mat4 psm_vp = glm::perspective<float>(1.57, 1, n - c->getNearClip(), n + 100) * glm::lookAt(p_pos, p_fwd, p_up);
		glm::mat4 psm_vp = glm::lookAt(p_pos, p_fwd, p_up);
		view = glm::lookAt<float>(
				applyHomo(psm_vp, position), 
				applyHomo(psm_vp, position + forward), 
				applyHomo(psm_vp, glm::vec3(0, 1, 0))) * psm_vp;
	}
	else if (sm_type == LIGHT_SM_TYPE_TRAPEZOID) FatalError("Trapezoidal SM not yet implemented").raise();
	else FatalError("Unrecognized SM type").raise();
	vp = projection * view;
}

void DirectionalLight::updateProj() {
	if (type == DIRECTIONAL_LIGHT_TYPE_ORTHO) {
		std::cout << "world-space focus: [<" << focus[0].x << ", " << focus[0].y << ", " << focus[0].z << ">, <";
		std::cout << focus[1].x << ", " << focus[1].y << ", " << focus[1].z << ">]\n";
		glm::vec3 temp = apply(view, focus[0]), ls_aabb[2] = {temp, temp};
		for (uint8_t i = 1; i < 8; i++) {
			temp = apply(view, glm::vec3(focus[i % 2].x, focus[(uint8_t)floor(i/2) % 2].y, focus[(uint8_t)floor(i/4) % 2].z));
			for (uint8_t j = 0; j < 3; j++) {
				if (temp[j] < ls_aabb[0][j]) ls_aabb[0][j] = temp[j];
				if (temp[j] > ls_aabb[1][j]) ls_aabb[1][j] = temp[j];
			}
		}
		// ls_aabb[0] -= LIGHT_SHADOW_AABB_FUDGE;
		// ls_aabb[1] += LIGHT_SHADOW_AABB_FUDGE;
		std::cout << "light-space ls_aabb: [<" << ls_aabb[0].x << ", " << ls_aabb[0].y << ", " << ls_aabb[0].z << ">, <";
		std::cout << ls_aabb[1].x << ", " << ls_aabb[1].y << ", " << ls_aabb[1].z << ">]\n";
		projection = glm::ortho<float>(
			ls_aabb[0].x, ls_aabb[1].x, 
			// ls_aabb[0].y, ls_aabb[1].y, 
			ls_aabb[1].y, ls_aabb[0].y,
			-(ls_aabb[0].z + 2 * (ls_aabb[1].z - ls_aabb[0].z)), -ls_aabb[0].z); // negate & flip b/c we're looking in the -z direction?
			// -(ls_aabb[1].z + 2 * (ls_aabb[0].z - ls_aabb[1].z)), -ls_aabb[1].z); // negate & flip b/c we're looking in the -z direction?
		glm::vec3 test[2] = {applyHomo(projection, ls_aabb[0]), applyHomo(projection, ls_aabb[1])};
		std::cout << "check test: [<" << test[0].x << ", " << test[0].y << ", " << test[0].z << ">, <";
			std::cout << test[1].x << ", " << test[1].y << ", " << test[1].z << ">]\n";
	}
	else if (type == DIRECTIONAL_LIGHT_TYPE_PERSP)
		projection = glm::perspective<float>(0.78, (float)shadowmap.extent.width / (float)shadowmap.extent.height, 0.01, 100);
	else FatalError("Unknown light projection type").raise();

	// projection[1][1] *= -1;
	vp = projection * view;
}

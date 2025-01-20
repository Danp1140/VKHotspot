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

Light::Light(LightInitInfo&& i) :
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

void Light::addVecToFocus(const glm::vec3& v) {
	for (uint8_t i = 0; i < 3; i++) {
		focus[0][i] = std::min(focus[0][i], v[i]);
		focus[1][i] = std::max(focus[1][i], v[i]);
	}
}

DirectionalLight::DirectionalLight(DirectionalLightInitInfo&& i) : 
	Light(std::move(i.super)), 
	type(i.t),
	forward(i.f) {
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
}

void DirectionalLight::addVecToFocus(const glm::vec3& v) {
	Light::addVecToFocus(v);
	updateProj();
}

void DirectionalLight::setPos(glm::vec3 p) {
	position = p;
	updateView();
}

void DirectionalLight::setForward(glm::vec3 f) {
	forward = f;
	updateView();
}

void DirectionalLight::updateView() {
	view = glm::lookAt<float>(position, position + forward, glm::vec3(0, 1, 0));
	vp = projection * view;
}

void DirectionalLight::updateProj() {
	glm::vec4 temp[2] = {view * glm::vec4(focus[0], 1), view * glm::vec4(focus[1], 1)};
	for (uint8_t i = 0; i < 2; i++) temp[i] /= temp[i].w;
	for (uint8_t i = 0; i < 3; i++) 
		if (temp[0][i] > temp[1][i]) std::swap(temp[0][i], temp[1][i]);
	
	if (type == DIRECTIONAL_LIGHT_TYPE_ORTHO) {
		temp[0] -= glm::vec4(LIGHT_SHADOW_AABB_FUDGE);
		temp[1] += glm::vec4(LIGHT_SHADOW_AABB_FUDGE);
		projection = glm::ortho<float>(
			temp[0].x, temp[1].x, 
			temp[0].y, temp[1].y, 
			// -temp[1].z, -temp[0].z);
			// TODO: this is still strange...
			0, 40);
/*
		std::cout << "lo-z: " << temp[0].z << std::endl;
		std::cout << "hi-z: " << temp[1].z << std::endl;
		std::cout << projection[0][0] << " " << projection[0][1] << " " << projection[0][2] << " " << projection[0][3] << std::endl;
		std::cout << projection[1][0] << " " << projection[1][1] << " " << projection[1][2] << " " << projection[1][3] << std::endl;
		std::cout << projection[2][0] << " " << projection[2][1] << " " << projection[2][2] << " " << projection[2][3] << std::endl;
		std::cout << projection[3][0] << " " << projection[3][1] << " " << projection[3][2] << " " << projection[3][3] << std::endl;
*/
	}
	else if (type == DIRECTIONAL_LIGHT_TYPE_PERSP) 
		projection = glm::perspective<float>(0.78, (float)shadowmap.extent.width / (float)shadowmap.extent.height, 0.01, 100);
	projection[1][1] *= -1;
	vp = projection * view;
}

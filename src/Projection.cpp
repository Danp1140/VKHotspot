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

Light::Light() {
	// TODO: init SM img if requested, for now we just do it by default at a default res
	shadowmap.extent = {1024, 1024};
	shadowmap.format = LIGHT_SHADOW_MAP_FORMAT;
	shadowmap.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	shadowmap.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	// ^^^ transitioned to DEPTH_STENCIL_READ_ONLY_OPTIMAL after renderpass
	vkCreateSampler(GH::getLD(), &defaultshadowsamplerci, nullptr, &shadowmap.sampler);
	GH::createImage(shadowmap);
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

DirectionalLight::DirectionalLight(glm::vec3 p, glm::vec3 f, glm::vec3 c) : forward(f), Light(p, c) { 
	updateView();
}

void DirectionalLight::updateView() {
	view = glm::lookAt<float>(position, position + forward, glm::vec3(0, 1, 0));
	vp = projection * view;
}

SunLight::SunLight(glm::vec3 p, glm::vec3 f, glm::vec3 c) : DirectionalLight(p, f, c) { 
	updateProj();
}

void SunLight::updateProj() {
	projection = glm::ortho<float>(-10, 10, -10, 10, 0, 20); 
	projection[1][1] *= -1;
	vp = projection * view;
}


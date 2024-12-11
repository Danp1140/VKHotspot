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


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

// -- Private --

void Camera::updateView() {
	
}

// starting with just a typical perspective cam
void Camera::updateProj() {
	projection = glm::perspective<float>(fovy, aspectratio, nearclip, farclip); 
}


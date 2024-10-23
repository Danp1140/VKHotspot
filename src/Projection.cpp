#include "Projection.h"

/*
 * Camera
 */

// -- Public --

Camera::Camera(glm::vec3 p, glm::vec3 f, float fov) : position(p), forward(f), fovy(fov) {
	updateMatrices();
}

// -- Private --

void Camera::updateMatrices() {
	
}

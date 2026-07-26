#include "Projection.h"

/*
 * ProjectionBase
 */

glm::vec3 ProjectionBase::apply(glm::mat4 A, glm::vec3 v) {
	// TODO get rid of unnecesary dot prod on fourth row
	glm::vec4 u = A * glm::vec4(v.x, v.y, v.z, 1);
	return glm::vec3(u.x, u.y, u.z);
}

glm::vec3 ProjectionBase::applyHomo(glm::mat4 A, glm::vec3 v) {
	glm::vec4 u = A * glm::vec4(v.x, v.y, v.z, 1);
	return glm::vec3(u.x, u.y, u.z) / u.w;
}

glm::mat4 ProjectionBase::ortho(float l, float r, float b, float t, float n, float f) {
	return glm::mat4(
		2 / (r-l), 0, 0, 0, // 1st col 
		0, 2 / (t-b), 0, 0, // 2nd col
		0, 0, -1 / (f-n), 0, // 3rd col
		(r+l)/(r-l), -(t+b)/(b-t), -n/(f-n), 1); // 4th col
}

/*
 * PositionalProjectionBase
 */

void PositionalProjectionBase::setPos(const glm::vec3& p) {
	position = p;
}

/*
 * DirectionalProjectionBase
 */

void DirectionalProjectionBase::setForward(const glm::vec3& f) {
	forward = glm::normalize(f);
}

/*
 * PerspectiveProjectionBase
 */

/*
 * Camera
 */

// -- Public --

Camera::Camera(const CameraInitInfo& ii) :
	PerspectiveProjectionBase(ii.perspective_super),
	PositionalProjectionBase(ii.positional_super),
	DirectionalProjectionBase(ii.directional_super) {
	updateView();
	updateProj();
}

void Camera::updateView() {
	view = glm::lookAt<float>(position, position + forward, glm::vec3(0, 1, 0));
	vp = projection * view;
}

void Camera::updateProj() {
	projection = glm::perspectiveRH_ZO<float>(fov_y, aspect_ratio, near_clip, far_clip); 
	projection[1][1] *= -1;
	vp = projection * view;
}

/*
 * LightSMData
 */

void LightSMData::addVecToFocus(const glm::vec3& v) {
	for (uint8_t i = 0; i < 3; i++) {
		focus[0][i] = std::min(focus[0][i], v[i]);
		focus[1][i] = std::max(focus[1][i], v[i]);
	}
}

void LightSMData::clearFocus() {
	focus[0] = glm::vec3(std::numeric_limits<float>::infinity());
	focus[1] = -glm::vec3(std::numeric_limits<float>::infinity());
}

/*
 * Light
 */

Light::Light(const LightInitInfo& i) : color(i.c) {}

void swap(Light& lhs, Light& rhs) {
	std::swap(lhs.color, rhs.color);
	std::swap(lhs.sm_data, rhs.sm_data);
}

void Light::addVecToFocus(const glm::vec3& v, size_t i) {
	sm_data[i].addVecToFocus(v);
}

/*
 * DirectionalLight
 */

/*
DirectionalLight& DirectionalLight::operator=(DirectionalLight&& rhs) {
	swap(*this, rhs);
	return *this;
}
*/

void swap(DirectionalLight& lhs, DirectionalLight& rhs) {
	swap(static_cast<Light&>(lhs), static_cast<Light&>(rhs));
}

void DirectionalLight::updateSMDatum(size_t sm_i, glm::vec3 up, glm::vec3* cam_AABB) {
	glm::mat4 view = glm::lookAt<float>(glm::vec3(0), forward, up);

	sm_data[sm_i].setView(view);

	glm::vec3 temp = ProjectionBase::apply(view, sm_data[sm_i].getFocus()[0]);
	glm::vec3 ls_aabb[2] = {temp, temp};
	for (uint8_t i = 1; i < 8; i++) {
		temp = ProjectionBase::apply(view, glm::vec3(
					sm_data[sm_i].getFocus()[i % 2].x, 
					sm_data[sm_i].getFocus()[(uint8_t)floor(i/2) % 2].y, 
					sm_data[sm_i].getFocus()[(uint8_t)floor(i/4) % 2].z));
		for (uint8_t j = 0; j < 3; j++) {
			if (temp[j] < ls_aabb[0][j]) ls_aabb[0][j] = temp[j];
			if (temp[j] > ls_aabb[1][j]) ls_aabb[1][j] = temp[j];
		}
	}
	if (cam_AABB) {
		temp = ProjectionBase::apply(view, cam_AABB[0]);
		glm::vec3 ls_cam_aabb[2] = {temp, temp};
		for (uint8_t i = 1; i < 8; i++) {
			temp = ProjectionBase::apply(view, cam_AABB[i]);
			for (uint8_t j = 0; j < 3; j++) {
				if (temp[j] < ls_cam_aabb[0][j]) ls_cam_aabb[0][j] = temp[j];
				if (temp[j] > ls_cam_aabb[1][j]) ls_cam_aabb[1][j] = temp[j];
			}
		}
		// TODO: could just use minimum of the two here
		ls_aabb[0].x = ls_cam_aabb[0].x;
		ls_aabb[1].x = ls_cam_aabb[1].x;
		ls_aabb[0].y = ls_cam_aabb[0].y;
		ls_aabb[1].y = ls_cam_aabb[1].y;
	}
	/*
	sm_data[sm_i].setProj(glm::orthoRH_ZO<float>(
		ls_aabb[0].x, ls_aabb[1].x,
		ls_aabb[1].y, ls_aabb[0].y,
		ls_aabb[0].z, ls_aabb[1].z));
		*/
	sm_data[sm_i].setProj(ProjectionBase::ortho(
		ls_aabb[0].x, ls_aabb[1].x,
		ls_aabb[0].y, ls_aabb[1].y,
		-ls_aabb[1].z, -ls_aabb[0].z));
	/*
	 * Why we flip and negate z here:
	 * We calculate a light-space z range.
	 * View transforms to look from origin toward -z.
	 * Ortho's near and far are positive numbers along this -z.
	 * So, we flip and negate to convert from +z to -z.
	 */

	sm_data[sm_i].updateProj();
}

void swap(SpotLight& lhs, SpotLight& rhs) {
	swap(static_cast<Light&>(lhs), static_cast<Light&>(rhs));
}

void SpotLight::updateSMDatum(size_t sm_i, glm::vec3 up, glm::vec3* cam_AABB) {
	glm::mat4 view = glm::lookAt<float>(position, position + forward, up);

	sm_data[sm_i].setView(view);

	glm::vec3 temp = ProjectionBase::apply(view, sm_data[sm_i].getFocus()[0]);
	glm::vec3 ls_aabb[2] = {temp, temp};
	for (uint8_t i = 1; i < 8; i++) {
		temp = ProjectionBase::apply(view, glm::vec3(
					sm_data[sm_i].getFocus()[i % 2].x, 
					sm_data[sm_i].getFocus()[(uint8_t)floor(i/2) % 2].y, 
					sm_data[sm_i].getFocus()[(uint8_t)floor(i/4) % 2].z));
		for (uint8_t j = 0; j < 3; j++) {
			if (temp[j] < ls_aabb[0][j]) ls_aabb[0][j] = temp[j];
			if (temp[j] > ls_aabb[1][j]) ls_aabb[1][j] = temp[j];
		}
	}
	if (cam_AABB) {
		temp = ProjectionBase::apply(view, cam_AABB[0]);
		glm::vec3 ls_cam_aabb[2] = {temp, temp};
		for (uint8_t i = 1; i < 8; i++) {
			temp = ProjectionBase::apply(view, cam_AABB[i]);
			for (uint8_t j = 0; j < 3; j++) {
				if (temp[j] < ls_cam_aabb[0][j]) ls_cam_aabb[0][j] = temp[j];
				if (temp[j] > ls_cam_aabb[1][j]) ls_cam_aabb[1][j] = temp[j];
			}
		}
		// TODO: could just use minimum of the two here
		ls_aabb[0].x = ls_cam_aabb[0].x;
		ls_aabb[1].x = ls_cam_aabb[1].x;
		ls_aabb[0].y = ls_cam_aabb[0].y;
		ls_aabb[1].y = ls_cam_aabb[1].y;
	}

	glm::mat4 p = glm::perspectiveRH_ZO<float>(
		fov_y, 
		aspect_ratio, 
	// 	-ls_aabb[1].z, -ls_aabb[0].z);
		near_clip, far_clip);

	/* 
	 * To get temp_aabb for real:
	 * project extrema into plane at near_clip normal to ±z,
	 * find where line from origin to extrema intersects this plane
	 * use these as bounds of frust
	 */

	temp = ls_aabb[0] * -near_clip / ls_aabb[0].z;
	glm::vec2 temp_aabb[2] = {glm::vec2(temp.x, temp.y), glm::vec2(temp.x, temp.y)};
	for (uint8_t i = 1; i < 8; i++) {
		temp = glm::vec3(
					ls_aabb[i % 2].x, 
					ls_aabb[(uint8_t)floor(i/2) % 2].y, 
					ls_aabb[(uint8_t)floor(i/4) % 2].z);
		temp *= -near_clip / temp.z;
		std::cout << temp.x << ", " << temp.y << ", " << temp.z << '\n';
		for (uint8_t j = 0; j < 2; j++) {
			if (temp[j] < temp_aabb[0][j]) temp_aabb[0][j] = temp[j];
			if (temp[j] > temp_aabb[1][j]) temp_aabb[1][j] = temp[j];
		}
	}

	sm_data[sm_i].setProj(glm::frustumRH_ZO<float>(
		temp_aabb[0].x, temp_aabb[1].x,
		temp_aabb[0].y, temp_aabb[1].y,
		near_clip, far_clip));

	sm_data[sm_i].updateProj();
}

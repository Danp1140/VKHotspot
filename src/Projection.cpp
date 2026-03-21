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

/*
 * PositionalProjectionBase
 */

void PositionalProjectionBase::setPos(const glm::vec3& p) {
	position = p;
	updateView();
}

/*
 * DirectionalProjectionBase
 */

void DirectionalProjectionBase::setForward(const glm::vec3& f) {
	forward = glm::normalize(f);
	updateView();
}

/*
 * Camera
 */

// -- Public --

Camera::Camera() : 
	fovy(glm::quarter_pi<float>()),
	aspectratio(1),
	nearclip(CAMERA_DEFAULT_NEAR_CLIP),
	farclip(CAMERA_DEFAULT_FAR_CLIP),
	PositionalProjectionBase(),
	DirectionalProjectionBase() {
	updateView();
	updateProj();
}

Camera::Camera(glm::vec3 p, glm::vec3 f, float fov, float ar) : 
	fovy(fov), 
	aspectratio(ar),
	nearclip(CAMERA_DEFAULT_NEAR_CLIP),
	farclip(CAMERA_DEFAULT_FAR_CLIP),
	PositionalProjectionBase(p),
	DirectionalProjectionBase(f) {
	updateView();
	updateProj();
}

void Camera::setFOVY(float f) {
	fovy = f;
	updateProj();
}

// -- Private --

void Camera::updateView() {
	view = glm::lookAt<float>(position, position + forward, glm::vec3(0, 1, 0));
	vp = projection * view;
}

void Camera::updateProj() {
	projection = glm::perspectiveRH_ZO<float>(fovy, aspectratio, nearclip, farclip); 
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

/*
 * Light
 */

Light::Light(const LightInitInfo& i) : color(i.c) {}

Light& Light::operator=(Light&& rhs) {
	swap(*this, rhs);
	return *this;
}

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

DirectionalLight::DirectionalLight(const DirectionalLightInitInfo& i) : 
	Light(i.super) {
	updateView();
	updateProj();
}

DirectionalLight& DirectionalLight::operator=(DirectionalLight&& rhs) {
	swap(*this, rhs);
	return *this;
}

void swap(DirectionalLight& lhs, DirectionalLight& rhs) {
	swap(static_cast<Light&>(lhs), static_cast<Light&>(rhs));
}

void DirectionalLight::updateSMDatum(size_t sm_i) {
	glm::mat4 view = glm::lookAt<float>(glm::vec3(0), forward, glm::vec3(0, 1, 0));
	sm_data[sm_i].setView(view);

	glm::vec3 temp = apply(view, sm_data[sm_i].focus[0]), ls_aabb[2] = {temp, temp};
	for (uint8_t i = 1; i < 8; i++) {
		temp = apply(view, glm::vec3(sm_data[sm_i].focus[i % 2].x, sm_data[sm_i].focus[(uint8_t)floor(i/2) % 2].y, sm_data[sm_i].focus[(uint8_t)floor(i/4) % 2].z));
		for (uint8_t j = 0; j < 3; j++) {
			if (temp[j] < ls_aabb[0][j]) ls_aabb[0][j] = temp[j];
			if (temp[j] > ls_aabb[1][j]) ls_aabb[1][j] = temp[j];
		}
	}
	sm_data[sm_i].setProj(glm::ortho<float>(
		ls_aabb[0].x, ls_aabb[1].x, 
		ls_aabb[1].y, ls_aabb[0].y,
		-(ls_aabb[0].z + 2 * (ls_aabb[1].z - ls_aabb[0].z)), -ls_aabb[0].z)); // negate & flip b/c we're looking in the -z direction?

	sm_data[sm_i].updateProj()
}

/*
void DirectionalLight::updateView() {
	view = glm::lookAt<float>(glm::vec3(0), forward, glm::vec3(0, 1, 0));
	vp = projection * view;
}
*/

/*
void DirectionalLight::updateProj() {
	if (sm_data.size() > 1) {
		Camera* c = (Camera*)sm_data;
		glm::mat4 ivp = glm::inverse(c->getVP());
		glm::vec3 coord = glm::vec3(-1, -1, 0);
		glm::vec3 temp = applyHomo(ivp, coord);
		temp = apply(view, temp);
		glm::vec3 ls_aabb[2] = {temp, temp};
		float f = 0.8;
		for (uint8_t i = 1; i < 8; i++) {
			coord.x = (i % 2 == 0) ? -1 : 1;
			coord.y = ((i/2) % 2 == 0) ? -1 : 1;
			coord.z = ((i/4) % 2 == 0) ? 0 : f;
			temp = applyHomo(ivp, coord);
			temp = apply(view, temp);
			for (uint8_t j = 0; j < 3; j++) {
				if (temp[j] < ls_aabb[0][j]) ls_aabb[0][j] = temp[j];
				if (temp[j] > ls_aabb[1][j]) ls_aabb[1][j] = temp[j];
			}
		}
		// ls_aabb[1].y = ls_aabb[0].y + (ls_aabb[1].y - ls_aabb[0].y) * f;
		projection = glm::ortho<float>(
			ls_aabb[0].x, ls_aabb[1].x,
			ls_aabb[1].y, ls_aabb[0].y,
			// -ls_aabb[1].z, -ls_aabb[0].z);
			0, 200);
		std::cout << ls_aabb[0].x << ", " << ls_aabb[1].x << "\n";
		std::cout << ls_aabb[0].y << ", " << ls_aabb[1].y << "\n";
		std::cout << ls_aabb[0].z << ", " << ls_aabb[1].z << "\n";
	}
	else {
		// std::cout << "world-space focus: [<" << focus[0].x << ", " << focus[0].y << ", " << focus[0].z << ">, <";
		// std::cout << focus[1].x << ", " << focus[1].y << ", " << focus[1].z << ">]\n";
		glm::vec3 temp = apply(view, sm_data.focus[0]), ls_aabb[2] = {temp, temp};
		for (uint8_t i = 1; i < 8; i++) {
			temp = apply(view, glm::vec3(sm_data.focus[i % 2].x, sm_data.focus[(uint8_t)floor(i/2) % 2].y, sm_data.focus[(uint8_t)floor(i/4) % 2].z));
			for (uint8_t j = 0; j < 3; j++) {
				if (temp[j] < ls_aabb[0][j]) ls_aabb[0][j] = temp[j];
				if (temp[j] > ls_aabb[1][j]) ls_aabb[1][j] = temp[j];
			}
		}
		// ls_aabb[0] -= LIGHT_SHADOW_AABB_FUDGE;
		// ls_aabb[1] += LIGHT_SHADOW_AABB_FUDGE;
		// std::cout << "light-space ls_aabb: [<" << ls_aabb[0].x << ", " << ls_aabb[0].y << ", " << ls_aabb[0].z << ">, <";
		// std::cout << ls_aabb[1].x << ", " << ls_aabb[1].y << ", " << ls_aabb[1].z << ">]\n";
		projection = glm::ortho<float>(
			ls_aabb[0].x, ls_aabb[1].x, 
			// ls_aabb[0].y, ls_aabb[1].y, 
			ls_aabb[1].y, ls_aabb[0].y,
			-(ls_aabb[0].z + 2 * (ls_aabb[1].z - ls_aabb[0].z)), -ls_aabb[0].z); // negate & flip b/c we're looking in the -z direction?
			// -(ls_aabb[1].z + 2 * (ls_aabb[0].z - ls_aabb[1].z)), -ls_aabb[1].z); // negate & flip b/c we're looking in the -z direction?
		// glm::vec3 test[2] = {applyHomo(projection, ls_aabb[0]), applyHomo(projection, ls_aabb[1])};
		// std::cout << "check test: [<" << test[0].x << ", " << test[0].y << ", " << test[0].z << ">, <";
		// std::cout << test[1].x << ", " << test[1].y << ", " << test[1].z << ">]\n";
	}
	// else if (type == DIRECTIONAL_LIGHT_TYPE_PERSP)
	// 	projection = glm::perspective<float>(0.78, (float)sm_data.extent.width / (float)sm_data.extent.height, 0.01, 100);
	// else FatalError("Unknown light projection type").raise();

	// projection[1][1] *= -1;
	vp = projection * view;
}
*/

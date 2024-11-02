#include <glm.hpp>
#include <ext.hpp>
#include <glm/gtc/constants.hpp>

#define CAMERA_DEFAULT_NEAR_CLIP 0.001
#define CAMERA_DEFAULT_FAR_CLIP 10000

class Camera {
public:
	Camera();
	Camera(glm::vec3 p, glm::vec3 f, float fov, float ar);
	~Camera() = default;

private:
	glm::vec3 position, forward;
	glm::mat4 view, projection;
	float fovy, aspectratio, nearclip, farclip;

	void updateView();
	void updateProj();
};

class Light {
public:
	Light() = default;
	Light(glm::vec3 p, glm::vec3 c) : position(p), color(c) {}
	~Light() = default;

private:
	glm::vec3 position;
	glm::vec3 color; // intensity baked-in to color, this is not normalized in the shader
};

class DirectionalLight : public Light {
public:
	DirectionalLight() = default;
	DirectionalLight(glm::vec3 p, glm::vec3 f, glm::vec3 c) : forward(f), Light(p, c) {}
	~DirectionalLight() = default;

private:
	glm::vec3 forward;
};

class SunLight : public Light {};

class PointLight : public Light {};

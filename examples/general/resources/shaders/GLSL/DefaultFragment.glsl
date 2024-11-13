#version 460

layout (location = 0) in vec3 ni;

layout (location = 0) out vec4 c;

void main() {
	c = vec4(0.5, 0.1, 0.8, 1) * (0.1 + max(0, dot(ni, vec3(0, 1, 0.5))));
	// c = vec4(ni, 1);
	c.a = 1;
}

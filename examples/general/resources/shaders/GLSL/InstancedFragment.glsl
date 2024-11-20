#version 460

layout (location = 0) in vec3 n;

layout (location = 0) out vec4 c;

void main() {
	const vec3 lp = vec3(0.5, 1, 0);

	c = vec4(vec3(0.9) * (0.1 + max(0, dot(n, lp))), 1);
}

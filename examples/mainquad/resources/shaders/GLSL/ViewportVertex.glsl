#version 460

layout (location = 0) in vec3 pi;
layout (location = 1) in vec2 ti;
layout (location = 2) in vec3 ni;

layout (push_constant) uniform Constants {
	mat4 vp;
} c;

layout (location = 0) out vec3 no;

void main() {
	gl_Position = c.vp * vec4(pi, 1);
	no = normalize(vec3(c.vp) * vec3(ni));
}

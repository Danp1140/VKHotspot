#version 460 

layout (push_constant) uniform Constants {
	mat4 vp;
	mat4 m;
} constants;

layout (location = 0) in vec3 pi;
layout (location = 1) in vec2 ti;
layout (location = 2) in vec3 ni;

layout (location = 0) out vec2 to;
layout (location = 1) out vec3 no;

void main() {
	gl_Position = constants.vp * constants.m * vec4(pi, 1);
	to = ti;
	no = mat3(constants.m) * ni;
}

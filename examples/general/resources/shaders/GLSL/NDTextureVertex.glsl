#version 460 

layout (location = 0) in vec3 pi;
layout (location = 1) in vec2 ti;
layout (location = 2) in vec3 ni;

layout (push_constant) uniform Constants {
	mat4 vp;
	mat4 m;
} c;

layout (location = 0) out vec2 to;
layout (location = 1) out vec3 no;

void main() {
	gl_Position = c.vp * c.m * vec4(pi, 1);
	to = ti;
	no = mat3(c.m) * ni;
}

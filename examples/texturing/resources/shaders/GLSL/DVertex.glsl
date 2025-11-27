#version 460

layout(location = 0) in vec3 pi;
layout(location = 1) in vec2 ti;
layout(location = 2) in vec3 ni;

layout(push_constant) uniform PushConstant {
	mat4 vp, m;
} pc;

layout(location = 0) out vec2 to;

void main() {
	gl_Position = pc.vp * pc.m * vec4(pi, 1);
	to = ti;
}

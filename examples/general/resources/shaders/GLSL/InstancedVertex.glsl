#version 460

#define MAX_IUB_LEN 8

layout (location = 0) in vec3 pi;
layout (location = 1) in vec2 ti;
layout (location = 2) in vec3 ni;

layout (push_constant) uniform Constants {
	mat4 vp;
} c;
layout (set = 0, binding = 0) uniform InstanceUB {
	mat4 m[MAX_IUB_LEN];
} iub;

layout (location = 0) out vec3 no;

void main() {
	gl_Position = c.vp * iub.m[gl_InstanceIndex] * vec4(pi, 1);
	no = normalize(vec3(iub.m[gl_InstanceIndex]) * vec3(ni));
}

#version 460

layout (location = 0) in vec2 ti;
layout (location = 1) in vec3 ni;

layout (set = 0, binding = 0) uniform sampler2D d;

layout (location = 0) out vec4 c;

void main() {
	c = texture(d, ti) * (0.1 + max(0, dot(ni, vec3(0, 1, 0.5))));
	c.a = 1;
}

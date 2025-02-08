#version 460

layout (location = 0) in vec2 ti;
layout (location = 1) in vec3 ni;

layout (push_constant) uniform Constants {
	mat4 vp;
	mat4 m;
} k;
layout (set = 0, binding = 0) uniform sampler2D ds;
layout (set = 0, binding = 1) uniform sampler2D ns;

layout (location = 0) out vec4 c;

void main() {
	// TODO: enforce vec3 norm tex
	// TODO: do face-wise tangent-space to model-space normal conversion
	//   multiply by mat3(t, n, t x n) 
	vec3 n = ni + mat3(k.m) * (texture(ns, ti).xyz * vec3(2) - vec3(1));
	c = texture(ds, ti) * (0.1 + max(0, dot(n, vec3(0, 1, 0.5))));
	c.a = 1;
}

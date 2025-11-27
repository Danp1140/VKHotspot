#version 460

layout(location = 0) in vec2 ti;

layout(set = 0, binding = 0) uniform sampler2D ds;

layout(location = 0) out vec4 co;

void main() {
	co = vec4(texture(ds, ti).xyz, 1);
}

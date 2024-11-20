#version 460

layout(push_constant) uniform Constants {
	vec4 bgcolor;
	vec2 position, extent;
	bool blend;
} constants;

layout(location = 0) in vec2 uv;
layout(location = 1) in vec2 pos;

layout(binding = 0) uniform sampler2D tex;

layout(location = 0) out vec4 color;

void main() {
	// at some point, may be worth differentiating text versus non-text shaders
	// then, in non-text we could specify tex or no-tex (avoid erroneous sampling)
	if (constants.blend) color = mix(constants.bgcolor, vec4(1, 1, 1, 1), texture(tex, uv).r);
	else color = texture(tex, uv);
}

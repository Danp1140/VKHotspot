#version 460

const uint UI_PC_FLAG_NONE  = 0x00u;
const uint UI_PC_FLAG_BLEND = 0x01u;
const uint UI_PC_FLAG_TEX   = 0x02u;

layout(push_constant) uniform Constants {
	vec4 bgcolor;
	vec2 position, extent;
	uint flags;
} c;

layout(location = 0) in vec2 uv;
layout(location = 1) in vec2 pos;

layout(binding = 0) uniform sampler2D tex;

layout(location = 0) out vec4 color;

void main() {
	// it's either branching or switching pipelines every two tris...i chose modest branching
	// bro what is GLSL's malfunction with bitwise operators...
	if ((c.flags & UI_PC_FLAG_TEX) != 0) {
		if ((c.flags & UI_PC_FLAG_BLEND) != 0) color = mix(c.bgcolor, vec4(1, 1, 1, 1), texture(tex, uv).r);
		else color = texture(tex, uv);
	}
	else color = c.bgcolor;
}

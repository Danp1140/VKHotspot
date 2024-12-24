#version 460

layout (constant_id = 0) const uint SCREEN_WIDTH = 1920;
layout (constant_id = 1) const uint SCREEN_HEIGHT = 1080;
vec2 SCREEN_VEC = vec2(SCREEN_WIDTH, SCREEN_HEIGHT); // cant make this const???

layout(push_constant) uniform Constants {
	vec4 bgcolor;
	vec2 position, extent;
	uint flags;
} constants;

// wouldn't let me make this const, may need to do some keyword hacking w/ defs of push constants
vec2 vertexpositions[4] = {
	(constants.position) / SCREEN_VEC * 2 - vec2(1),
	(constants.position + vec2(constants.extent.x, 0)) / SCREEN_VEC * 2 - vec2(1), 
	(constants.position + constants.extent) / SCREEN_VEC * 2 - vec2(1), 
	(constants.position + vec2(0, constants.extent.y)) / SCREEN_VEC * 2 - vec2(1) 
};
const vec2 vertexuvs[4] = {
    vec2(0., 1.),
    vec2(1., 1.),
    vec2(1., 0.),
    vec2(0., 0.)
};
const uint vertexindices[6] = {
    0, 1, 2,
    2, 3, 0
};

layout(location = 0) out vec2 uv;
layout(location = 1) out vec2 pos;

void main() {
	pos = vertexpositions[vertexindices[gl_VertexIndex]];
	gl_Position = vec4(pos, 0, 1);
	uv = vertexuvs[vertexindices[gl_VertexIndex]];
}

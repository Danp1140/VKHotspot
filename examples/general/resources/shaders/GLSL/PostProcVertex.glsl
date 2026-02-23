#version 460

const vec2 vertexpositions[4]={
    vec2(-1., -1.),
    vec2(1., -1.),
    vec2(1., 1.),
    vec2(-1., 1.)
};
const vec2 vertexuvs[4]={
    vec2(0., 0.),
    vec2(1., 0.),
    vec2(1., 1.),
    vec2(0., 1.)
};
const uint vertexindices[6]={
    0, 2, 1,
    2, 0, 3
};

layout(location=0) out vec2 uvthru;

void main(){
	gl_Position=vec4(vertexpositions[vertexindices[gl_VertexIndex]], 0., 1.);
	uvthru=vertexuvs[vertexindices[gl_VertexIndex]];
}

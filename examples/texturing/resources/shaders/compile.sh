glslc --target-env=vulkan1.0 -fshader-stage=vert GLSL/DVertex.glsl -o SPIRV/dvert.spv
glslc --target-env=vulkan1.0 -fshader-stage=frag GLSL/DFragment.glsl -o SPIRV/dfrag.spv

glslc --target-env=vulkan1.0 -fshader-stage=vert GLSL/DNSVertex.glsl -o SPIRV/dnsvert.spv
glslc --target-env=vulkan1.0 -fshader-stage=frag GLSL/DNSFragment.glsl -o SPIRV/dnsfrag.spv

echo "Compiling Viewport Shaders"
glslc -fshader-stage=vert GLSL/ViewportVertex.glsl -o SPIRV/viewportvert.spv
glslc -fshader-stage=frag GLSL/ViewportFragment.glsl -o SPIRV/viewportfrag.spv

echo "Compiling Instanced Viewport Shaders"
glslc -fshader-stage=vert GLSL/ViewportInstancedVertex.glsl -o SPIRV/viewportinstancedvert.spv
glslc -fshader-stage=frag GLSL/ViewportInstancedFragment.glsl -o SPIRV/viewportinstancedfrag.spv


echo "Compiling UI Shaders"
glslc -fshader-stage=vert GLSL/UIVertex.glsl -o SPIRV/UIvert.spv
glslc -fshader-stage=frag GLSL/UIFragment.glsl -o SPIRV/UIfrag.spv

echo "Compiling Default Shaders"
glslc -fshader-stage=vert GLSL/DefaultVertex.glsl -o SPIRV/defaultvert.spv
glslc -fshader-stage=frag GLSL/DefaultFragment.glsl -o SPIRV/defaultfrag.spv

echo "Compiling Instanced Shaders"
glslc -fshader-stage=vert GLSL/InstancedVertex.glsl -o SPIRV/instancedvert.spv
glslc -fshader-stage=frag GLSL/InstancedFragment.glsl -o SPIRV/instancedfrag.spv

echo "Compiling Diffuse Texture Shaders"
glslc -fshader-stage=vert GLSL/DiffuseTextureVertex.glsl -o SPIRV/diffusetexturevert.spv
glslc -fshader-stage=frag GLSL/DiffuseTextureFragment.glsl -o SPIRV/diffusetexturefrag.spv


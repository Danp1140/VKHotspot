echo "Compiling Default Shaders"
glslc -fshader-stage=vert GLSL/DefaultVertex.glsl -o SPIRV/defaultvert.spv
glslc -fshader-stage=frag GLSL/DefaultFragment.glsl -o SPIRV/defaultfrag.spv

echo "Compiling Instanced Shaders"
glslc -fshader-stage=vert GLSL/InstancedVertex.glsl -o SPIRV/instancedvert.spv
glslc -fshader-stage=frag GLSL/InstancedFragment.glsl -o SPIRV/instancedfrag.spv

echo "Compiling Diffuse Texture Shaders"
glslc -fshader-stage=vert GLSL/DiffuseTextureVertex.glsl -o SPIRV/diffusetexturevert.spv
glslc -fshader-stage=frag GLSL/DiffuseTextureFragment.glsl -o SPIRV/diffusetexturefrag.spv

echo "Compiling ND Texture Shaders"
glslc -fshader-stage=vert GLSL/NDTextureVertex.glsl -o SPIRV/NDtexturevert.spv
glslc -fshader-stage=frag GLSL/NDTextureFragment.glsl -o SPIRV/NDtexturefrag.spv

echo "Compiling Volumetric Shader"
glslc -fshader-stage=comp GLSL/VolumeCompute.glsl -o SPIRV/volcomp.spv

echo "Compiling Post-Processing Shaders"
glslc -fshader-stage=vert GLSL/PostProcVertex.glsl -o SPIRV/postprocvert.spv
glslc -fshader-stage=frag GLSL/PostProcFragment.glsl -o SPIRV/postprocfrag.spv

echo "Compiling POM"
glslc -fshader-stage=vert GLSL/NDPVertex.glsl -o SPIRV/ndpvert.spv
glslc -fshader-stage=frag GLSL/NDPFragment.glsl -o SPIRV/ndpfrag.spv


#ifndef MESH_H
#define MESH_H

#include <gtc/quaternion.hpp>

#include "GraphicsHandler.h"
class Mesh;
#include "Scene.h"

typedef uint16_t MeshIndex;

typedef enum VertexBufferTraitBits {
	VERTEX_BUFFER_TRAIT_POSITION = 0x01,
	VERTEX_BUFFER_TRAIT_UV = 0x02,
	VERTEX_BUFFER_TRAIT_NORMAL = 0x04,
	VERTEX_BUFFER_TRAIT_WEIGHT = 0x08,
} VertexBufferTraitBits;
typedef uint8_t VertexBufferTraits;
#define MAX_VERTEX_BUFFER_NUM_TRAITS 4

typedef struct MeshPCData {
	glm::mat4 m;
} MeshPCData;

class Mesh {
public:
	Mesh() : 
		position(glm::vec3(0)),
		scale(glm::vec3(1)),
		rotation(glm::vec3(0)),
		model(glm::mat4(1)),
		vbtraits(VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL) {}
	Mesh(const char* f);
	~Mesh();

	virtual void recordDraw(
		VkFramebuffer f, 
		VkRenderPass rp,
		RenderSet rs,
		size_t rsidx,
		VkCommandBuffer& c) const;
	static size_t getTraitsElementSize(VertexBufferTraits t);
	static VkPipelineVertexInputStateCreateInfo getVISCI(VertexBufferTraits t);
	static void ungetVISCI(VkPipelineVertexInputStateCreateInfo v);

	const glm::mat4& getModelMatrix() const {return model;}
	const BufferInfo getVertexBuffer() const {return vertexbuffer;}
	const BufferInfo getIndexBuffer() const {return indexbuffer;}

	void setPos(glm::vec3 p);
	void setRot(glm::quat r);
	void setScale(glm::vec3 s);

protected:
	BufferInfo vertexbuffer, indexbuffer;
	static VkDeviceSize vboffsettemp;

	/*
	 * Also creates buffers, so make sure there aren't valid buffers that will get
	 * lost in vertexbuffer and indebuffer before you call this
	 */
	void loadOBJ(const char* fp);

private:
	// TODO: make position scale & rotation actually affect model, and send model over to shader
	glm::vec3 position, scale;
	glm::quat rotation;
	glm::mat4 model;
	VertexBufferTraits vbtraits;

	size_t getVertexBufferElementSize() const;
	
	void updateModelMatrix();
};

typedef struct InstancedMeshData {
	glm::mat4 m;
} InstancedMeshData;

class InstancedMesh : public Mesh {
public:
	InstancedMesh() = default;
	InstancedMesh(const char* fp, std::vector<InstancedMeshData> m);
	~InstancedMesh();

	/*
	 * TODO: should instanced mesh still use other position rotation etc stuff? to just apply to all
	 * of them?
	 */

	const BufferInfo& getInstanceUB() const {return instanceub;}
	/*
	 * Reuses buffer if it's the same size, otherwise recreates
	 * TODO: implement more detailed buffer update functions in GH [l]
	 */
	void updateInstanceUB(std::vector<InstancedMeshData> m);

	void recordDraw(
		VkFramebuffer f, 
		VkRenderPass rp,
		RenderSet rs,
		size_t rsidx,
		VkCommandBuffer& c) const;

private:
	BufferInfo instanceub;
};

class LODMesh {
public:
	LODMesh() = default;
	~LODMesh() = default;

	void recordDraw(
		VkFramebuffer f, 
		VkRenderPass rp,
		RenderSet rs,
		size_t rsidx,
		VkCommandBuffer& c) const;


private:
	Mesh* meshes;
	uint8_t nummeshes;
	// if multiple should draw, will draw highest LOD
	std::function<bool ()> shouldload, shoulddraw;
};

/*
 * Things you may want to draw with Mesh
 *  - simple, static mesh (w/ vertex position, probably requiring uvs and normals as well)
 *  - armatured (possibly animated) mesh
 *  - particle system-associated mesh (likely instanced rendering)
 *  - various textures for fragment shader
 *  - height or displacement mapped, tessellated mesh
 *  - volumetrics???
 *  - shadow casting??
 *  - shadow receiving??
 *  - dynamic vs. static shadows?????
 *  - editable mesh with vertices kept accessible, as well as edge and face references
 *
 * Common between all of these:
 *  - requires a pipeline and basic pos, scale, rot
 *  - requires at least vertex positions 
 * If you want lambertian, spec, other effects, you need normals
 * If you want any texture, you need uvs
 * If you want armatures, you need weights
 */

#endif

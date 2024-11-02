#include "GraphicsHandler.h"

typedef uint16_t MeshIndex;

typedef enum VertexBufferTraitBits {
	VERTEX_BUFFER_TRAIT_POSITION = 0x01,
	VERTEX_BUFFER_TRAIT_UV = 0x02,
	VERTEX_BUFFER_TRAIT_NORMAL = 0x04,
	VERTEX_BUFFER_TRAIT_WEIGHT = 0x08,
} VertexBufferTraitBits;
typedef uint8_t VertexBufferTraits;
#define MAX_VERTEX_BUFFER_NUM_TRAITS 4

class Mesh {
public:
	Mesh() : vbtraits(VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL) {}
	Mesh(const char* f);
	~Mesh();

	static void recordDraw(cbRecData d, VkCommandBuffer& c);
	static size_t getTraitsElementSize(VertexBufferTraits t);
	static VkPipelineVertexInputStateCreateInfo getVISCI(VertexBufferTraits t);
	static void ungetVISCI(VkPipelineVertexInputStateCreateInfo v);

	const BufferInfo getVertexBuffer() const {return vertexbuffer;}
	const BufferInfo getIndexBuffer() const {return indexbuffer;}
	const PipelineInfo& getGraphicsPipeline() const {return pipeline;}
	const VkDescriptorSet getDS() const {return ds;}

private:
	glm::vec3 position, scale;
	glm::quat rotation;
	glm::mat4 model;
	BufferInfo vertexbuffer, indexbuffer;
	VertexBufferTraits vbtraits;
	PipelineInfo pipeline;
	VkDescriptorSet ds;

	size_t getVertexBufferElementSize() const;
	
	/*
	 * Also creates buffers, so make sure there aren't valid buffers that will get
	 * lost in vertexbuffer and indebuffer before you call this
	 */
	void loadOBJ(const char* fp);
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

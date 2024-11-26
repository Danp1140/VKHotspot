#include <gtc/quaternion.hpp>

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

typedef struct MeshDrawData {
	VkRenderPass r;
	VkPipeline p;
	VkPipelineLayout pl;
	VkPushConstantRange pcr;
	const void* pcd;
	VkPushConstantRange opcr;
	const void* opcd;
	VkDescriptorSet d;
	VkBuffer vb, ib;
	size_t nv;
} MeshDrawData;

typedef std::function<void (VkFramebuffer f, const MeshDrawData d, VkCommandBuffer& c)> MeshDrawFunc;

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
		vbtraits(VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL), 
		drawfunc(Mesh::recordDraw) {}
	Mesh(const char* f);
	~Mesh();

	// TODO: specialized move/copy assign and move/copy initializers
	Mesh& operator=(Mesh m);

	static void recordDraw(VkFramebuffer f, const MeshDrawData d, VkCommandBuffer& c);
	static size_t getTraitsElementSize(VertexBufferTraits t);
	static VkPipelineVertexInputStateCreateInfo getVISCI(VertexBufferTraits t);
	static void ungetVISCI(VkPipelineVertexInputStateCreateInfo v);

	// TODO; reevaluate the usefulness of this function
	const MeshDrawData getDrawData(
		VkRenderPass r, 
		const PipelineInfo& p, 
		VkPushConstantRange pcr, 
		const void* pcd,
		VkPushConstantRange opcr,
		const void* opcd,
		VkDescriptorSet ds) const;

	const glm::mat4& getModelMatrix() const {return model;}
	const BufferInfo getVertexBuffer() const {return vertexbuffer;}
	const BufferInfo getIndexBuffer() const {return indexbuffer;}
	const MeshDrawFunc& getDrawFunc() const {return drawfunc;}

	void setPos(glm::vec3 p);

protected:
	MeshDrawFunc drawfunc;
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
	BufferInfo vertexbuffer, indexbuffer;
	VertexBufferTraits vbtraits;

	size_t getVertexBufferElementSize() const;
	
	void updateModelMatrix();
};

typedef struct InstancedMeshData {
	glm::mat4 m;
} InstancedMeshData;

class InstancedMesh : public Mesh {
public:
	InstancedMesh();
	InstancedMesh(const char* fp, std::vector<InstancedMeshData> m);
	~InstancedMesh();

	InstancedMesh& operator=(InstancedMesh rhs);

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

	static void recordDraw(VkFramebuffer f, const MeshDrawData d, VkCommandBuffer& c);

private:
	BufferInfo instanceub;
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

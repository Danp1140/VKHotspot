#ifndef MESH_H
#define MESH_H

#include <gtc/quaternion.hpp>

#include "GraphicsHandler.h"
class MeshBase;
class Mesh;
#include "Scene.h"

class MeshBase {
public:
	MeshBase() : 
		position(0),
		scale(1),
		rotation(1, 0, 0, 0),
		model(1) {}
	MeshBase(const MeshBase& lvalue) = delete;
	MeshBase(MeshBase&& rvalue) :
		position(std::move(rvalue.position)),
		scale(std::move(rvalue.scale)),
		rotation(std::move(rvalue.rotation)),
		model(std::move(rvalue.model)) {}
	~MeshBase() = default;

	friend void swap(MeshBase& lhs, MeshBase& rhs);

	MeshBase& operator=(const MeshBase& rhs) = delete;
	MeshBase& operator=(MeshBase&& rhs);

	virtual void recordDraw(
		VkFramebuffer f, 
		VkRenderPass rp,
		RenderSet rs,
		size_t rsidx,
		VkCommandBuffer& c) const = 0;

	const glm::vec3& getPos() const {return position;}
	const glm::mat4& getModelMatrix() const {return model;}

	void setPos(glm::vec3 p);
	void setRot(glm::quat r);
	void setScale(glm::vec3 s);

private:
	glm::vec3 position, scale;
	glm::quat rotation;
	glm::mat4 model;

	void updateModelMatrix();
};

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

class Mesh : public MeshBase {
public:
	Mesh() : MeshBase(),
		vbtraits(VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL) {}
	Mesh(const Mesh& lvalue) = delete;
	Mesh(Mesh&& rvalue);
	Mesh(const char* f);
	~Mesh();

	friend void swap(Mesh& lhs, Mesh& rhs);

	Mesh& operator=(const Mesh& rhs) = delete;
	Mesh& operator=(Mesh&& rhs);

	virtual void recordDraw(
		VkFramebuffer f, 
		VkRenderPass rp,
		RenderSet rs,
		size_t rsidx,
		VkCommandBuffer& c) const;
	static size_t getTraitsElementSize(VertexBufferTraits t);
	static VkPipelineVertexInputStateCreateInfo getVISCI(VertexBufferTraits t);
	static void ungetVISCI(VkPipelineVertexInputStateCreateInfo v);

	const BufferInfo getVertexBuffer() const {return vertexbuffer;}
	const BufferInfo getIndexBuffer() const {return indexbuffer;}

protected:
	BufferInfo vertexbuffer, indexbuffer;
	static VkDeviceSize vboffsettemp;

	/*
	 * Also creates buffers, so make sure there aren't valid buffers that will get
	 * lost in vertexbuffer and indebuffer before you call this
	 */
	void loadOBJ(const char* fp);

private:
	VertexBufferTraits vbtraits;

	size_t getVertexBufferElementSize() const;
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

typedef bool(*LODFunc)(Mesh&, void*);

class LODMeshData {
public:
	LODMeshData() = default;
	LODMeshData(const LODMeshData& lvalue) = delete;
	LODMeshData(LODMeshData&& rvalue) :
		m(std::move(rvalue.m)),
		shouldload(std::move(rvalue.shouldload)),
		shoulddraw(std::move(rvalue.shoulddraw)),
		sldata(std::move(rvalue.sldata)),
		sddata(std::move(rvalue.sddata)) {}
	LODMeshData(Mesh&& me, LODFunc sl, LODFunc sd, void* sld, void* sdd) :
		m(std::move(me)),
		shouldload(sl),
		shoulddraw(sd),
		sldata(sld),
		sddata(sdd) {}
	~LODMeshData() = default;

	friend void swap(LODMeshData& lhs, LODMeshData& rhs);

	LODMeshData& operator=(const LODMeshData& rhs) = delete;
	LODMeshData& operator=(LODMeshData&& rhs);

	bool shouldLoad() {return shouldload(m, sldata);}
	void load() {} // TODO: actually implement loading system in Mesh class
	bool shouldDraw() {return shoulddraw(m, sddata);}

	const Mesh& getMesh() {return m;}

private:
	Mesh m;
	LODFunc shouldload, shoulddraw;
	/*
	bool(*shouldload)(Mesh&, void*);
	bool(*shoulddraw)(Mesh&, void*);
	*/
	void* sldata, * sddata;
};

// a little janky, Mesh's buffers go unused but this is most intuitive for the user i think
// consider a BaseMesh class that Mesh & LODMesh inherits from with pure virtual recordDraw,
// model mat etc., but no buffers
class LODMesh : public MeshBase {
public:
	LODMesh() : meshes(nullptr), nummeshes(0) {}
	LODMesh(std::vector<LODMeshData>& md);
	~LODMesh();

	void recordDraw(
		VkFramebuffer f, 
		VkRenderPass rp,
		RenderSet rs,
		size_t rsidx,
		VkCommandBuffer& c) const;

private:
	// if multiple should draw, will draw first found. behavior can be implicitly controlled by
	// order of meshes
	LODMeshData* meshes;
	uint8_t nummeshes;
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

#include "Mesh.h"

VkDeviceSize Mesh::vboffsettemp = 0;

void swap(MeshBase& lhs, MeshBase& rhs) {
	std::swap(lhs.position, rhs.position);
	std::swap(lhs.scale, rhs.scale);
	std::swap(lhs.rotation, rhs.rotation);
	std::swap(lhs.model, rhs.model);
}

MeshBase& MeshBase::operator=(MeshBase&& rhs) {
	swap(*this, rhs);
	return *this;
}

void MeshBase::setPos(glm::vec3 p) {
	position = p;
	updateModelMatrix();
}

void MeshBase::setRot(glm::quat r) {
	rotation = r;
	updateModelMatrix();
}

void MeshBase::setScale(glm::vec3 s) {
	scale = s;
	updateModelMatrix();
}

void MeshBase::updateModelMatrix() {
	// TODO: maximize efficiency, this feels like it could be made way better
	model = glm::translate(glm::mat4(1), position)
		* glm::mat4_cast(rotation)
		* glm::scale(glm::mat4(1), scale);
}

Mesh::Mesh(Mesh&& rvalue) :
	MeshBase(std::move(rvalue)),
	vbtraits(std::move(rvalue.vbtraits)),
	vertexbuffer(std::move(rvalue.vertexbuffer)),
	indexbuffer(std::move(rvalue.indexbuffer)) {
	rvalue.vertexbuffer = {};
	rvalue.indexbuffer = {};
}

Mesh::Mesh(const char* f) : Mesh() {
	loadOBJ(f);
}

Mesh::Mesh(VertexBufferTraits vbt, size_t vbs, size_t ibs, VkBufferUsageFlags abu) : vbtraits(vbt) {
	vertexbuffer.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | abu;
	vertexbuffer.size = getVertexBufferElementSize() * vbs;
	GH::createBuffer(vertexbuffer);
	indexbuffer.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | abu;
	indexbuffer.size = sizeof(MeshIndex) * ibs;
	GH::createBuffer(indexbuffer);
}

Mesh::~Mesh() {
	if (vertexbuffer.buffer != VK_NULL_HANDLE) GH::destroyBuffer(vertexbuffer);
	if (indexbuffer.buffer != VK_NULL_HANDLE) GH::destroyBuffer(indexbuffer);
}

void swap(Mesh& lhs, Mesh& rhs) {
	swap(static_cast<MeshBase&>(lhs), static_cast<MeshBase&>(rhs));
	std::swap(lhs.vbtraits, rhs.vbtraits);
	std::swap(lhs.vertexbuffer, rhs.vertexbuffer);
	std::swap(lhs.indexbuffer, rhs.indexbuffer);
}

Mesh& Mesh::operator=(Mesh&& rhs) {
	swap(*this, rhs);
	return *this;
}

void Mesh::recordDraw(
	VkFramebuffer f, 
	VkRenderPass rp,
	RenderSet rs,
	size_t rsidx,
	VkCommandBuffer& c) const {
	VkCommandBufferInheritanceInfo cbinherinfo {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		nullptr,
		rp, 0,
		f,
		VK_FALSE, 0, 0
	};
	VkCommandBufferBeginInfo cbbi {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		nullptr,
		VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
		&cbinherinfo
	};
	vkBeginCommandBuffer(c, &cbbi);
	vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_GRAPHICS, rs.pipeline.pipeline);
	if (rs.objdss[rsidx] != VK_NULL_HANDLE) {
		vkCmdBindDescriptorSets(
			c,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			rs.pipeline.layout,
			0, 1, &rs.objdss[rsidx],
			0, nullptr);
	}
	if (rs.pcdata) 
		vkCmdPushConstants(
			c, 
			rs.pipeline.layout, 
			rs.pipeline.pushconstantrange.stageFlags, 
			rs.pipeline.pushconstantrange.offset, 
			rs.pipeline.pushconstantrange.size, 
			rs.pcdata);
	if (rs.objpcdata[rsidx]) 
		vkCmdPushConstants(
			c, 
			rs.pipeline.layout, 
			rs.pipeline.objpushconstantrange.stageFlags, 
			rs.pipeline.objpushconstantrange.offset, 
			rs.pipeline.objpushconstantrange.size, 
			rs.objpcdata[rsidx]);
	vkCmdBindVertexBuffers(c, 0, 1, &vertexbuffer.buffer, &vboffsettemp);
	vkCmdBindIndexBuffer(c, indexbuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(c, indexbuffer.size / sizeof(MeshIndex), 1, 0, 0, 0);
	vkEndCommandBuffer(c);
}

size_t Mesh::getTraitsElementSize(VertexBufferTraits t) {
	size_t result = 0;
	if (t & VERTEX_BUFFER_TRAIT_POSITION) result += sizeof(glm::vec3);
	if (t & VERTEX_BUFFER_TRAIT_UV) result += sizeof(glm::vec2);
	if (t & VERTEX_BUFFER_TRAIT_NORMAL) result += sizeof(glm::vec3);
	// WEIGHT is included in override from ArmaturedMesh
	return result;
}

VkPipelineVertexInputStateCreateInfo Mesh::getVISCI(VertexBufferTraits t, VertexBufferTraits o) {
	uint32_t numtraits = 0, offset = 0;
	VkVertexInputBindingDescription* bindingdesc = new VkVertexInputBindingDescription[1] {
		{0, static_cast<uint32_t>(getTraitsElementSize(t)), VK_VERTEX_INPUT_RATE_VERTEX}
	};
	VkVertexInputAttributeDescription* attribdesc = new VkVertexInputAttributeDescription[MAX_VERTEX_BUFFER_NUM_TRAITS];
	if (t & VERTEX_BUFFER_TRAIT_POSITION) {
		if (!(o & VERTEX_BUFFER_TRAIT_POSITION)) {
			attribdesc[numtraits] = {numtraits, 0, VK_FORMAT_R32G32B32_SFLOAT, offset};
			numtraits++;
		}
		offset += sizeof(glm::vec3);
	}
	if (t & VERTEX_BUFFER_TRAIT_UV) {
		if (!(o & VERTEX_BUFFER_TRAIT_UV)) {
			attribdesc[numtraits] = {numtraits, 0, VK_FORMAT_R32G32_SFLOAT, offset};
			numtraits++;
		}
		offset += sizeof(glm::vec2);
	}
	if (t & VERTEX_BUFFER_TRAIT_NORMAL) {
		if (!(o & VERTEX_BUFFER_TRAIT_NORMAL)) {
			attribdesc[numtraits] = {numtraits, 0, VK_FORMAT_R32G32B32_SFLOAT, offset};
			numtraits++;
		}
		offset += sizeof(glm::vec3);
	}
	if (t & VERTEX_BUFFER_TRAIT_WEIGHT) {
		FatalError("Vertex weight not yet supported").raise();
	}
	return (VkPipelineVertexInputStateCreateInfo){
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		nullptr,
		0,
		1, bindingdesc,
		numtraits, attribdesc
	};
}

void Mesh::ungetVISCI(VkPipelineVertexInputStateCreateInfo v) {
	delete[] v.pVertexBindingDescriptions;
	delete[] v.pVertexAttributeDescriptions;
}

// could maybe make this constexpr
size_t Mesh::getVertexBufferElementSize() const {
	return getTraitsElementSize(vbtraits);
}

void Mesh::loadOBJ(const char* fp) {
	std::vector<unsigned int> vertexindices, uvindices, normalindices;
	std::vector<glm::vec3> vertextemps, normaltemps;
	std::vector<glm::vec2> uvtemps;
	FILE* obj = fopen(fp, "r");
	if (!obj) FatalError("Couldn't open OBJ file").raise();
	int expectedmatches = 0;
	if (vbtraits & VERTEX_BUFFER_TRAIT_POSITION) expectedmatches += 3;
	if (vbtraits & VERTEX_BUFFER_TRAIT_UV) expectedmatches += 3;
	if (vbtraits & VERTEX_BUFFER_TRAIT_NORMAL) expectedmatches += 3;
	char lineheader[128];
	int res, matches;
	glm::vec3 vertex;
	glm::vec2 uv;
	glm::vec3 normal;
	unsigned int vertidx[3], uvidx[3], normidx[3];
	while (true) {
		res = fscanf(obj, "%s", lineheader);
		if (res == EOF) break;
		if (vbtraits & VERTEX_BUFFER_TRAIT_POSITION && strcmp(lineheader, "v") == 0) {
			fscanf(obj, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z);
			vertextemps.push_back(vertex);
		} else if (vbtraits & VERTEX_BUFFER_TRAIT_UV && strcmp(lineheader, "vt") == 0) {
			fscanf(obj, "%f %f\n", &uv.x, &uv.y);
			uvtemps.push_back(uv);
		} else if (vbtraits & VERTEX_BUFFER_TRAIT_NORMAL && strcmp(lineheader, "vn") == 0) {
			fscanf(obj, "%f %f %f\n", &normal.x, &normal.y, &normal.z);
			normaltemps.push_back(normal);
		} else if (strcmp(lineheader, "f") == 0) {
			matches = fscanf(obj, "%d/%d/%d %d/%d/%d %d/%d/%d", 
					&vertidx[0], &uvidx[0], &normidx[0],
					&vertidx[1], &uvidx[1], &normidx[1],
					&vertidx[2], &uvidx[2], &normidx[2]);
			if (matches != expectedmatches) {
				FatalError("Malformed OBJ file").raise();
				return;
			}
			for (auto& v: vertidx) vertexindices.push_back(v - 1);
			for (auto& n: normidx) normalindices.push_back(n - 1);
			for (auto& u: uvidx) uvindices.push_back(u - 1);
		}
	}
	vertexbuffer.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	vertexbuffer.size = getVertexBufferElementSize() * vertexindices.size();
	GH::createBuffer(vertexbuffer);
	// what if we did buffer usage tracking in GH???
	indexbuffer.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	indexbuffer.size = sizeof(MeshIndex) * vertexindices.size();
	GH::createBuffer(indexbuffer);
	void* vdst = malloc(vertexbuffer.size),
		* idst = malloc(indexbuffer.size);
	char* vscan = static_cast<char*>(vdst),
		* iscan = static_cast<char*>(idst);
	MeshIndex itemp;
	for (uint16_t x = 0; x < vertexindices.size() / 3; x++) {
		for (uint16_t y = 0; y < 3; y++) {
			if (vbtraits & VERTEX_BUFFER_TRAIT_POSITION) {
				memcpy(vscan, &vertextemps[vertexindices[3 * x + y]], sizeof(glm::vec3));
				vscan += sizeof(glm::vec3);
			}
			if (vbtraits & VERTEX_BUFFER_TRAIT_UV) {
				memcpy(vscan, &uvtemps[uvindices[3 * x + y]], sizeof(glm::vec2));
				vscan += sizeof(glm::vec2);
			}
			if (vbtraits & VERTEX_BUFFER_TRAIT_NORMAL) {
				memcpy(vscan, &normaltemps[normalindices[3 * x + y]], sizeof(glm::vec3));
				vscan += sizeof(glm::vec3);
			}
			if (vbtraits & VERTEX_BUFFER_TRAIT_WEIGHT) {
				FatalError("Vertex weight loading from OBJ file not yet supported").raise();
			}
			itemp = 3 * x + y;
			memcpy(iscan, &itemp, sizeof(MeshIndex));
			iscan += sizeof(MeshIndex);
		}
	}
	GH::updateWholeBuffer(vertexbuffer, vdst);
	free(vdst);
	GH::updateWholeBuffer(indexbuffer, idst);
	free(idst);
}

InstancedMesh::InstancedMesh(InstancedMesh&& rvalue) :
	Mesh(std::move(rvalue)),
	instanceub(std::move(rvalue.instanceub)) {
	rvalue.instanceub = {};
}

InstancedMesh::InstancedMesh(const char* fp, std::vector<InstancedMeshData> m) : InstancedMesh() {
	loadOBJ(fp);
	instanceub.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	instanceub.size = m.size() * sizeof(InstancedMeshData);
	GH::createBuffer(instanceub);
	GH::updateWholeBuffer(instanceub, m.data());
}

InstancedMesh::~InstancedMesh() {
	GH::destroyBuffer(instanceub);
}

void swap(InstancedMesh& lhs, InstancedMesh& rhs) {
	swap(static_cast<Mesh&>(lhs), static_cast<Mesh&>(rhs));
	std::swap(lhs.instanceub, rhs.instanceub);
}

InstancedMesh& InstancedMesh::operator=(InstancedMesh&& rhs) {
	swap(*this, rhs);
	return *this;
}

void InstancedMesh::updateInstanceUB(std::vector<InstancedMeshData> m) {
	if (m.size() * sizeof(InstancedMeshData) == instanceub.size) {
		GH::updateWholeBuffer(instanceub, m.data());
		return;
	}
	GH::destroyBuffer(instanceub);
	instanceub.size = m.size() * sizeof(InstancedMeshData);
	GH::createBuffer(instanceub);
	GH::updateWholeBuffer(instanceub, m.data());
}

void InstancedMesh::recordDraw(
		VkFramebuffer f, 
		VkRenderPass rp,
		RenderSet rs,
		size_t rsidx,
		VkCommandBuffer& c) const {
	VkCommandBufferInheritanceInfo cbinherinfo {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		nullptr,
		rp, 0,
		f,
		VK_FALSE, 0, 0
	};
	VkCommandBufferBeginInfo cbbi {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		nullptr,
		VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
		&cbinherinfo
	};
	vkBeginCommandBuffer(c, &cbbi);
	vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_GRAPHICS, rs.pipeline.pipeline);
	if (rs.objdss[rsidx] != VK_NULL_HANDLE) {
		vkCmdBindDescriptorSets(
			c,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			rs.pipeline.layout,
			0, 1, &rs.objdss[rsidx],
			0, nullptr);
	}
	if (rs.pcdata) 
		vkCmdPushConstants(
			c, 
			rs.pipeline.layout, 
			rs.pipeline.pushconstantrange.stageFlags, 
			rs.pipeline.pushconstantrange.offset, 
			rs.pipeline.pushconstantrange.size, 
			rs.pcdata);
	if (rs.objpcdata[rsidx]) 
		vkCmdPushConstants(
			c, 
			rs.pipeline.layout, 
			rs.pipeline.objpushconstantrange.stageFlags, 
			rs.pipeline.objpushconstantrange.offset, 
			rs.pipeline.objpushconstantrange.size, 
			rs.objpcdata[rsidx]);
	vkCmdBindVertexBuffers(c, 0, 1, &vertexbuffer.buffer, &vboffsettemp);
	vkCmdBindIndexBuffer(c, indexbuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(c, indexbuffer.size / sizeof(MeshIndex), instanceub.size / sizeof(InstancedMeshData), 0, 0, 0);
	vkCmdDrawIndexed(c, indexbuffer.size / sizeof(MeshIndex), 1, 0, 0, 0);
	vkEndCommandBuffer(c);
}

LODMesh::LODMesh(std::vector<LODMeshData>& md) : nummeshes(md.size()) {
	meshes = new LODMeshData[nummeshes];
	for (uint8_t i = 0; i < nummeshes; i++) {
		meshes[i] = std::move(md[i]);
	}
}

LODMesh::~LODMesh() {
	delete[] meshes;
}

void swap(LODMeshData& lhs, LODMeshData& rhs) {
	swap(lhs.m, rhs.m);
	std::swap(lhs.shouldload, rhs.shouldload);
	std::swap(lhs.shoulddraw, rhs.shoulddraw);
	std::swap(lhs.sldata, rhs.sldata);
	std::swap(lhs.sddata, rhs.sddata);
}

LODMeshData& LODMeshData::operator=(LODMeshData&& rhs) {
	/*
	m = std::move(rhs.m);
	shouldload = std::move(rhs.shouldload);
	shoulddraw = std::move(rhs.shoulddraw);
	sldata = std::move(rhs.sldata);
	sddata = std::move(rhs.sddata);
	*/
	swap(*this, rhs);
	return *this;
}

void LODMesh::recordDraw(
		VkFramebuffer f, 
		VkRenderPass rp,
		RenderSet rs,
		size_t rsidx,
		VkCommandBuffer& c) const {
	uint8_t i;
	for (i = 0; i < nummeshes; i++) {
		if (meshes[i].shouldLoad()) {
			meshes[i].load();
		}
	}
	for (i = 0; i < nummeshes; i++) {
		if (meshes[i].shouldDraw()) {
			meshes[i].getMesh().recordDraw(f, rp, rs, rsidx, c);
			break;
		}
	}
}

#include "Mesh.h"

VkDeviceSize Mesh::vboffsettemp = 0;

Mesh::Mesh(const char* f) : Mesh() {
	loadOBJ(f);
}

Mesh::~Mesh() {
	if (vertexbuffer.buffer != VK_NULL_HANDLE) GH::destroyBuffer(vertexbuffer);
	if (indexbuffer.buffer != VK_NULL_HANDLE) GH::destroyBuffer(indexbuffer);
}

void Mesh::recordDraw(VkFramebuffer f, MeshDrawData d, VkCommandBuffer& c) {
	VkCommandBufferInheritanceInfo cbinherinfo {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		nullptr,
		d.r, 0,
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
	vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_GRAPHICS, d.p);
	if (d.d != VK_NULL_HANDLE) {
		vkCmdBindDescriptorSets(
			c,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			d.pl,
			0, 1, &d.d,
			0, nullptr);
	}
	vkCmdPushConstants(c, d.pl, d.pcr.stageFlags, d.pcr.offset, d.pcr.size, d.pcd);
	vkCmdPushConstants(c, d.pl, d.opcr.stageFlags, d.opcr.offset, d.opcr.size, d.opcd);
	vkCmdBindVertexBuffers(c, 0, 1, &d.vb, &vboffsettemp);
	vkCmdBindIndexBuffer(c, d.ib, 0, VK_INDEX_TYPE_UINT16);
	vkCmdDrawIndexed(c, d.nv, 1, 0, 0, 0);
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

VkPipelineVertexInputStateCreateInfo Mesh::getVISCI(VertexBufferTraits t) {
	uint32_t numtraits = 0, offset = 0;
	VkVertexInputBindingDescription* bindingdesc = new VkVertexInputBindingDescription[1] {
		{0, static_cast<uint32_t>(getTraitsElementSize(t)), VK_VERTEX_INPUT_RATE_VERTEX}
	};
	VkVertexInputAttributeDescription* attribdesc = new VkVertexInputAttributeDescription[MAX_VERTEX_BUFFER_NUM_TRAITS];
	if (t & VERTEX_BUFFER_TRAIT_POSITION) {
		attribdesc[numtraits] = {numtraits, 0, VK_FORMAT_R32G32B32_SFLOAT, offset};
		numtraits++;
		offset += sizeof(glm::vec3);
	}
	if (t & VERTEX_BUFFER_TRAIT_UV) {
		attribdesc[numtraits] = {numtraits, 0, VK_FORMAT_R32G32_SFLOAT, offset};
		numtraits++;
		offset += sizeof(glm::vec2);
	}
	if (t & VERTEX_BUFFER_TRAIT_NORMAL) {
		attribdesc[numtraits] = {numtraits, 0, VK_FORMAT_R32G32B32_SFLOAT, offset};
		numtraits++;
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

const MeshDrawData Mesh::getDrawData(
	VkRenderPass r, 
	const PipelineInfo& p, 
	VkPushConstantRange pcr, 
	const void* pcd,
	VkPushConstantRange opcr,
	const void* opcd,
	VkDescriptorSet ds) const {
	return (MeshDrawData){
		r,
		p.pipeline,
		p.layout,
		pcr,
		pcd,
		opcr,
		opcd,
		ds,
		vertexbuffer.buffer, indexbuffer.buffer,
		indexbuffer.size / sizeof(MeshIndex)
	};
}

void Mesh::setPos(glm::vec3 p) {
	position = p;
	updateModelMatrix();
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

void Mesh::updateModelMatrix() {
	// TODO: maximize efficiency, this feels like it could be made way better
	model = glm::translate(glm::mat4(1), position)
		* glm::mat4_cast(rotation)
		* glm::scale(glm::mat4(1), scale);
}

InstancedMesh::InstancedMesh() : Mesh() {
	drawfunc = InstancedMesh::recordDraw;
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

void InstancedMesh::recordDraw(VkFramebuffer f, MeshDrawData d, VkCommandBuffer& c) {
	VkCommandBufferInheritanceInfo cbinherinfo {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		nullptr,
		d.r, 0,
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
	vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_GRAPHICS, d.p);
	vkCmdBindDescriptorSets(
		c,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		d.pl,
		0, 1, &d.d,
		0, nullptr);
	vkCmdPushConstants(c, d.pl, d.pcr.stageFlags, d.pcr.offset, d.pcr.size, d.pcd);
	vkCmdBindVertexBuffers(c, 0, 1, &d.vb, &vboffsettemp);
	vkCmdBindIndexBuffer(c, d.ib, 0, VK_INDEX_TYPE_UINT16);
	vkCmdDrawIndexed(c, d.nv, 32, 0, 0, 0);
	vkEndCommandBuffer(c);
}


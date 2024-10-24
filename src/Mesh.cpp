#include "Mesh.h"

Mesh::Mesh(const char* f) : Mesh() {
	loadOBJ(f);
}

Mesh::~Mesh() {
	if (vertexbuffer.buffer != VK_NULL_HANDLE) GH::destroyBuffer(vertexbuffer);
	if (indexbuffer.buffer != VK_NULL_HANDLE) GH::destroyBuffer(indexbuffer);
}

// could maybe make this constexpr
size_t Mesh::getVertexBufferElementSize() const {
	size_t result = 0;
	if (vbtraits & VERTEX_BUFFER_TRAIT_POSITION) result += sizeof(glm::vec3);
	if (vbtraits & VERTEX_BUFFER_TRAIT_UV) result += sizeof(glm::vec2);
	if (vbtraits & VERTEX_BUFFER_TRAIT_NORMAL) result += sizeof(glm::vec3);
	// WEIGHT is included in override from ArmaturedMesh
	return result;
}

void Mesh::loadOBJ(const char* fp) {
	// TODO: further optimizations like not reallocing inside each loop
	// can be done once we actually render to screen
	std::vector<unsigned int> vertexindices, uvindices, normalindices;
	std::vector<glm::vec3> vertextemps, normaltemps;
	std::vector<glm::vec2> uvtemps;
	FILE* obj = fopen(fp, "r");
	if (!obj) FatalError("Couldn't open OBJ file").raise();
	int expectedmatches = 0;
	if (vbtraits & VERTEX_BUFFER_TRAIT_POSITION) expectedmatches += 3;
	if (vbtraits & VERTEX_BUFFER_TRAIT_UV) expectedmatches += 3;
	if (vbtraits & VERTEX_BUFFER_TRAIT_NORMAL) expectedmatches += 3;
	while (true) {
		char lineheader[128];
		int res = fscanf(obj, "%s", lineheader);
		if (res == EOF) break;
		if (vbtraits & VERTEX_BUFFER_TRAIT_POSITION && strcmp(lineheader, "v") == 0) {
			glm::vec3 vertex;
			fscanf(obj, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z);
			vertextemps.push_back(vertex);
		} else if (vbtraits & VERTEX_BUFFER_TRAIT_UV && strcmp(lineheader, "vt") == 0) {
			glm::vec2 uv;
			fscanf(obj, "%f %f\n", &uv.x, &uv.y);
			uvtemps.push_back(uv);
		} else if (vbtraits & VERTEX_BUFFER_TRAIT_NORMAL && strcmp(lineheader, "vn") == 0) {
			glm::vec3 normal;
			fscanf(obj, "%f %f %f\n", &normal.x, &normal.y, &normal.z);
			normaltemps.push_back(normal);
		} else if (strcmp(lineheader, "f") == 0) {
			unsigned int vertidx[3], uvidx[3], normidx[3];
			int matches = fscanf(obj, "%d/%d/%d %d/%d/%d %d/%d/%d", &vertidx[0], &uvidx[0], &normidx[0],
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

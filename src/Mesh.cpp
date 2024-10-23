#include "Mesh.h"

Mesh::Mesh(const char* f) : Mesh() {
	loadOBJ(f);
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
	// tris = std::vector<Tri>();
	// vertices = std::vector<Vertex>();
	std::vector<unsigned int> vertexindices, uvindices, normalindices;
	std::vector<glm::vec3> vertextemps, normaltemps;
	std::vector<glm::vec2> uvtemps;
	FILE* obj = fopen(fp, "r");
	if (!obj) FatalError("Couldn't open OBJ file").raise();
	int expectedmatches = 0;
	if (vbtraits & VERTEX_BUFFER_TRAIT_POSITION) expectedmatches += 3;
	if (vbtraits & VERTEX_BUFFER_TRAIT_UV) expectedmatches += 3;
	if (vbtraits & VERTEX_BUFFER_TRAIT_NORMAL) expectedmatches += 3;
	// TODO: further optimizations like not reallocing inside each loop
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

	void* vdst = malloc(getVertexBufferElementSize() * vertexindices.size()),
		* idst = malloc(sizeof(MeshIndex) * vertexindices.size());
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
	// GH::updateBuffer(vertexbuffer, vdst);
	free(vdst);
	// GH::updateBuffer(indexbuffer, idst);
	free(idst);
}

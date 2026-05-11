#include "Geometry.h"

AABB::AABB() {
	bounds[0] = glm::vec3(std::numeric_limits<float>::infinity());
	bounds[1] = -glm::vec3(std::numeric_limits<float>::infinity());
}

AABB::AABB(glm::vec3 min, glm::vec3 max) {
	bounds[0] = min;
	bounds[1] = max;
}

void AABB::add(glm::vec3 v) {
	for (uint8_t i = 0; i < 3; i++) {
		if (v[i] < bounds[0][i]) bounds[0][i] = v[i];
		if (v[i] > bounds[1][i]) bounds[1][i] = v[i];
	}
}

void AABB::clear() {
	bounds[0] = glm::vec3(std::numeric_limits<float>::infinity());
	bounds[1] = -glm::vec3(std::numeric_limits<float>::infinity());
}

glm::vec3 AABB::getCenter() const {
	return (bounds[0] + bounds[1]) / 2.f;
}

bool AABB::overlaps(const AABB& other) {
	bool res = false;
	for (uint8_t i = 0; i < 3; i++) {
		if ((bounds[0][i] > other.getBounds()[0][i] && bounds[0][i] < other.getBounds()[1][i])
				|| (bounds[1][i] > other.getBounds()[0][i] && bounds[1][i] < other.getBounds()[1][i])) {
			res = true;
			break;
		}
	}
	return res;
}

Octree::Octree(const std::vector<Mesh*> m, const std::vector<InstancedMesh*> im, uint8_t d) :
	meshes(m),
	inst_meshes(im),
	depth(d) {
	for (const MeshBase* m : meshes) {
		aabb.add(m->getAABB()[0]);
		aabb.add(m->getAABB()[1]);
	}

	calculateChildren();
}

Octree::Octree(const AABB& a, const std::vector<Mesh*> m, const std::vector<InstancedMesh*> im, uint8_t d) :
	aabb(a),
	meshes(m),
	inst_meshes(im),
	depth(d) {
	calculateChildren();
}

Octree::~Octree() {
	if (children) delete[] children;
}

void Octree::frustumCull(const glm::mat4& f) {
	if (depth == 0) {
		for (Mesh* m : meshes) m->enableDraw();
		// all meshes should draw
	}
	else {
		for (uint8_t i = 0; i < 8; i++) {
			if (children[i].intersectsFrust(f)) children[i].frustumCull(f);
		}
	}
}

void Octree::calculateChildren() {
	if (children) delete[] children;
	if (depth == 0) {
		children = nullptr;
		return;
	}
	children = new Octree[8];
	std::vector<Mesh*> next_meshes;
	AABB next_aabb;
	for (uint8_t i = 0; i < 8; i++) {
		next_meshes.clear();
		next_aabb.clear();
		next_aabb.add(glm::vec3(
			aabb.getBounds()[i % 2].x, 
			aabb.getBounds()[(uint8_t)floor(i/2) % 2].y, 
			aabb.getBounds()[(uint8_t)floor(i/4) % 2].z));
		next_aabb.add(aabb.getCenter());

		for (Mesh* m : meshes) {
			// if in node n, add to that nodes constructor list
			if (next_aabb.overlaps(AABB(m->getAABB()[0], m->getAABB()[1]))) {
				next_meshes.push_back(m);
			}
		}
		// TODO: do instanced too
		children[i] = Octree(next_aabb, next_meshes, {}, depth-1);
	}
}

bool Octree::intersectsFrust(const glm::mat4& f) {
	glm::vec3 temp;
	for (uint8_t i = 0; i < 8; i++) {
		temp = glm::vec3(
			aabb.getBounds()[i % 2].x, 
			aabb.getBounds()[(uint8_t)floor(i/2) % 2].y, 
			aabb.getBounds()[(uint8_t)floor(i/4) % 2].z);
		temp = ProjectionBase::applyHomo(f, temp);
		for (uint8_t j = 0; j < 3; j++) {
			if (temp[j] >= -1 && temp[j] <= 1) return true;
		}
	}
	return false;
}

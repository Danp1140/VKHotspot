#include "Geometry.h"

void AABB::add(glm::vec3 v) {
	for (uint8_t i = 0; i < 3; i++) {
		if (v[i] < bounds[0][i]) bounds[0][i] = v[i];
		if (v[i] > bounds[1][i]) bounds[1][i] = v[i];
	}
}

void AABB::clear() {
	bounds = {
		glm::vec3(std::numeric_limits<float>::infinity()), 
		-glm::vec3(std::numeric_limits<float>::infinity())
	};
}

glm::vec3 AABB::getCenter() const {
	return (bounds[0] + bounds[1]) / 2;
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

Octree::Octree(const std::vector<const MeshBase*> meshes, uint8_t d) : AABB(), depth(d) {
	for (const MeshBase* m : meshes) {
		AABB.add(m->getAABB()[0]);
		AABB.add(m->getAABB()[1]);
	}

	calculateChildren();
}

Octree(const AABB& a, const std::vector<const MeshBase*> meshes, uint8_t d) : AABB(a), depth(d) r
	calculateChildren();
}

void Octree::frustumCull(const glm::mat4& f) {
	if (depth = 0) {
		
		// all meshes should draw
	}
	else {
		for (uint8_t i = 0; i < 8; i++) {
			if (children[i].intersectsFrust(f)) children[i].frustumCull(f);
		}
	}
}

void Octree::calculateChildren() {
	std::vector<const MeshBase*> next_meshes;
	AABB next_aabb;
	for (uint8_t i = 0; i < 8; i++) {
		next_meshes.clear();
		next_aabb.clear();
		next_aabb.add(glm::vec3(
			aabb.getBounds()[i % 2].x, 
			aabb.getBounds()[(uint8_t)floor(i/2) % 2].y, 
			aabb.getBounds()[(uint8_t)floor(i/4) % 2].z));
		next_aabb.add(aabb.getCenter());

		for (const MeshBase* m : meshes) {
			// if in node n, add to that nodes constructor list
			if (next_aabb.overlaps(AABB(m->getAABB()[0], m->getAABB()[1]))) {
				next_meshes.push_back(m);
			}
		}
		nodes[i] = Octree(next_aabb, next_meshes, depth-1);
	}
}

bool Octree::intersectsFrust(const glm::mat4& f) {
	glm::vec3 temp;
	for (uint8_t i = 0; i < 8; i++) {
		temp = glm::vec3(
			aabb.getBounds()[i % 2].x, 
			aabb.getBounds()[(uint8_t)floor(i/2) % 2].y, 
			aabb.getBounds()[(uint8_t)floor(i/4) % 2].z));
		temp = ProjectionBase::applyHomo(f, temp);
		for (uint8_t j = 0; j < 3; j++) {
			if (temp[j] >= -1 && temp[j] <= 1) return true;
		}
	}
	return false;
}

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
	for (uint8_t i = 0; i < 3; i++) {
		if (bounds[1][i] < other.bounds[0][i] || other.bounds[1][i] < bounds[0][i]) 
			return false;
	}
	return true;
}

AABB AABB::apply(const glm::mat4& m) {
	AABB res;
	for (uint8_t i = 0; i < 8; i++) {
		res.add(ProjectionBase::apply(m, glm::vec3(
			bounds[i % 2].x, 
			bounds[(uint8_t)floor(i/2) % 2].y, 
			bounds[(uint8_t)floor(i/4) % 2].z)));
	}
	return res;
}

AABB AABB::applyHomo(const glm::mat4& m) {
	AABB res;
	for (uint8_t i = 0; i < 8; i++) {
		res.add(ProjectionBase::applyHomo(m, glm::vec3(
			bounds[i % 2].x, 
			bounds[(uint8_t)floor(i/2) % 2].y, 
			bounds[(uint8_t)floor(i/4) % 2].z)));
	}
	return res;
}

Octree::Octree(const Octree& lvalue) :
	aabb(lvalue.aabb),
	depth(lvalue.depth),
	children(nullptr),
	meshes(lvalue.meshes),
	inst_meshes(lvalue.inst_meshes),
	inst_idxs(lvalue.inst_idxs),
	flags(OCTREE_FLAG_BITS_NONE) {
	if (lvalue.children) {
		children = new Octree[8];
		for (uint8_t i = 0; i < 8; i++)
			children[i] = lvalue.children[i];
	}
}

Octree::Octree(Octree&& rvalue) :
	aabb(std::move(rvalue.aabb)),
	depth(std::move(rvalue.depth)),
	children(rvalue.children),
	meshes(std::move(rvalue.meshes)),
	inst_meshes(std::move(rvalue.inst_meshes)),
	inst_idxs(std::move(rvalue.inst_idxs)),
	flags(OCTREE_FLAG_BITS_NONE) {
	rvalue.children = nullptr;
}

Octree::Octree(const std::vector<Mesh*> m, const std::vector<InstancedMesh*> im, uint8_t d) :
	meshes(m),
	inst_meshes(im),
	depth(d),
	children(nullptr),
	flags(OCTREE_FLAG_BITS_NONE) {
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
	depth(d),
	children(nullptr),
	flags(OCTREE_FLAG_BITS_NONE) {
	calculateChildren();
}

Octree::~Octree() {
	if (children) {
		delete[] children;
		children = nullptr;
	}
}

Octree& Octree::operator=(Octree rhs) {
	swap(*this, rhs);
	return *this;
}

void swap(Octree& lhs, Octree& rhs) {
	std::swap(lhs.aabb, rhs.aabb);
	std::swap(lhs.depth, rhs.depth);
	std::swap(lhs.children, rhs.children);
	std::swap(lhs.meshes, rhs.meshes);
	std::swap(lhs.inst_meshes, rhs.inst_meshes);
	std::swap(lhs.inst_idxs, rhs.inst_idxs);
	std::swap(lhs.flags, rhs.flags);
}

void Octree::frustumCull(const glm::mat4& v, const glm::mat4& p, std::map<const MeshBase*, bool>& cull_map) {
	if (containedByFrust(v, p)) 
		cullNone(cull_map);
	else if (intersectsFrust(v, p)) {
		if (depth == 0) 
			cullNone(cull_map);
		else {
			flags &= ~(OCTREE_FLAG_BITS_ALL_CULLED | OCTREE_FLAG_BITS_NONE_CULLED);
			for (uint8_t i = 0; i < 8; i++)
				children[i].frustumCull(v, p, cull_map);
		}
	}
	else 
		cullAll(cull_map);
}

void Octree::setCheap() {
	flags |= OCTREE_FLAG_BITS_CHEAP;
	if (depth > 0) {
		for (uint8_t i = 0; i < 8; i++) children[i].setCheap();
	}
}

void Octree::calculateChildren() {
	if (depth == 0) {
		children = nullptr;
		return;
	}
	if (!children) {
		children = new Octree[8];
	}
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
			if (next_aabb.overlaps(AABB(m->getAABB()[0], m->getAABB()[1]).apply(m->getModelMatrix()))) {
				next_meshes.push_back(m);
			}
		}
		// TODO: do instanced too
		children[i] = Octree(next_aabb, next_meshes, {}, depth-1);
	}
}

bool Octree::intersectsFrust(const glm::mat4& v, const glm::mat4& p) {
	if (flags & OCTREE_FLAG_BITS_CHEAP) return cheapIntersectsFrust(v, p);
	glm::mat4 p_inv = glm::inverse(p);
	glm::vec3 frust_edges[4] = {
		glm::normalize(ProjectionBase::applyHomo(p_inv, glm::vec3(-1, -1, 1)) - ProjectionBase::applyHomo(p, glm::vec3(-1, -1, 0))),
		glm::normalize(ProjectionBase::applyHomo(p_inv, glm::vec3(-1, 1, 1)) - ProjectionBase::applyHomo(p, glm::vec3(-1, 1, 0))),
		glm::normalize(ProjectionBase::applyHomo(p_inv, glm::vec3(1, -1, 1)) - ProjectionBase::applyHomo(p, glm::vec3(1, -1, 0))),
		glm::normalize(ProjectionBase::applyHomo(p_inv, glm::vec3(1, 1, 1)) - ProjectionBase::applyHomo(p, glm::vec3(1, 1, 0)))
	};
	glm::vec3 cube_faces[3] = {
		glm::normalize(ProjectionBase::apply(v, glm::vec3(1, 0, 0))),
		glm::normalize(ProjectionBase::apply(v, glm::vec3(0, 1, 0))),
		glm::normalize(ProjectionBase::apply(v, glm::vec3(0, 0, 1)))
	};
	glm::vec3 frust_faces[5] = {
		glm::vec3(0, 0, 1),
		glm::cross(frust_edges[0], glm::vec3(0, 1, 0)),
		glm::cross(frust_edges[0], glm::vec3(1, 0, 0)),
		glm::cross(frust_edges[3], glm::vec3(0, 1, 0)),
		glm::cross(frust_edges[3], glm::vec3(1, 0, 0))
	};
	glm::vec3 axes[26] = {
		cube_faces[0],
		cube_faces[1],
		cube_faces[2],
		frust_faces[0],
		frust_faces[1],
		frust_faces[2],
		frust_faces[3],
		frust_faces[4],
		glm::cross(cube_faces[0], glm::vec3(0, 1, 0)),
		glm::cross(cube_faces[1], glm::vec3(0, 1, 0)),
		glm::cross(cube_faces[2], glm::vec3(0, 1, 0)),
		glm::cross(cube_faces[0], glm::vec3(1, 0, 0)),
		glm::cross(cube_faces[1], glm::vec3(1, 0, 0)),
		glm::cross(cube_faces[2], glm::vec3(1, 0, 0)),
		glm::cross(cube_faces[0], frust_edges[0]),
		glm::cross(cube_faces[1], frust_edges[0]),
		glm::cross(cube_faces[2], frust_edges[0]),
		glm::cross(cube_faces[0], frust_edges[1]),
		glm::cross(cube_faces[1], frust_edges[1]),
		glm::cross(cube_faces[2], frust_edges[1]),
		glm::cross(cube_faces[0], frust_edges[2]),
		glm::cross(cube_faces[1], frust_edges[2]),
		glm::cross(cube_faces[2], frust_edges[2]),
		glm::cross(cube_faces[0], frust_edges[3]),
		glm::cross(cube_faces[1], frust_edges[3]),
		glm::cross(cube_faces[2], frust_edges[3]),
	};
	for (uint8_t i = 0; i < 26; i++) {
		if (axisTest(axes[i], v, p_inv)) return false;
	}
	return true;
}

bool Octree::containedByFrust(const glm::mat4& v, const glm::mat4& p) {
	glm::mat4 vp = p * v;
	glm::vec3 temp;
	for (uint8_t i = 0; i < 8; i++) {
		temp = ProjectionBase::applyHomo(vp, glm::vec3(
			aabb.getBounds()[i % 2].x, 
			aabb.getBounds()[(uint8_t)floor(i/2) % 2].y, 
			aabb.getBounds()[(uint8_t)floor(i/4) % 2].z));
		if (temp[0] > 1 || temp[0] < -1) return false;
		if (temp[1] > 1 || temp[1] < -1) return false;
		if (temp[2] > 1 || temp[2] < 0) return false;
	}
	return true;
}

bool Octree::cheapIntersectsFrust(const glm::mat4& v, const glm::mat4& p) {
	glm::mat4 vp = p * v;
	AABB frust_aabb(glm::vec3(-1, -1, 0), glm::vec3(1, 1, 1));
	return aabb.apply(p * v).overlaps(frust_aabb);
}

void Octree::cullAll(std::map<const MeshBase*, bool>& cull_map) {
	if (flags & OCTREE_FLAG_BITS_ALL_CULLED) return;
	if (depth == 0) {
		for (Mesh* m : meshes)
			cull_map[m] = false;
	}
	else {
		for (uint8_t i = 0; i < 8; i++) children[i].cullAll(cull_map);
	}
	flags |= OCTREE_FLAG_BITS_ALL_CULLED;
}

void Octree::cullNone(std::map<const MeshBase*, bool>& cull_map) {
	if (flags & OCTREE_FLAG_BITS_NONE_CULLED) return;
	if (depth == 0) {
		for (Mesh* m : meshes) 
			cull_map[m] = true;
	}
	else {
		for (uint8_t i = 0; i < 8; i++) children[i].cullNone(cull_map);
	}
	flags |= OCTREE_FLAG_BITS_NONE_CULLED;

}

bool Octree::axisTest(glm::vec3 a, const glm::mat4& v, const glm::mat4& p_inv) {
	float temp = glm::dot(a, ProjectionBase::apply(v, aabb.getBounds()[0]));
	float aabb_bounds[2] = {temp, temp};
	for (uint8_t i = 1; i < 8; i++) {
		temp = glm::dot(a, ProjectionBase::apply(v, glm::vec3(
			aabb.getBounds()[i % 2].x, 
			aabb.getBounds()[(uint8_t)floor(i/2) % 2].y, 
			aabb.getBounds()[(uint8_t)floor(i/4) % 2].z)));
		aabb_bounds[0] = fmin(aabb_bounds[0], temp);
		aabb_bounds[1] = fmax(aabb_bounds[1], temp);
	}
	temp = glm::dot(a, ProjectionBase::applyHomo(p_inv, glm::vec3(-1)));
	float f_bounds[2] = {temp, temp};
	for (uint8_t i = 1; i < 8; i++) {
		temp = glm::dot(a, ProjectionBase::applyHomo(p_inv, glm::vec3(
			i % 2 == 0 ? -1 : 1, 
			(uint8_t)floor(i/2) % 2 == 0 ? -1 : 1, 
			(uint8_t)floor(i/4) % 2 == 0 ? -1 : 1)));
		f_bounds[0] = fmin(f_bounds[0], temp);
		f_bounds[1] = fmax(f_bounds[1], temp);
	}

	return aabb_bounds[1] < f_bounds[0] || f_bounds[1] < aabb_bounds[0];
}

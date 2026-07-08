#ifndef GEOMETRY_H
#define GEOMETRY_H

class AABB;
class Octree;

#include "Projection.h"
#include "Mesh.h"

class AABB {
public:
	AABB();
	AABB(glm::vec3 min, glm::vec3 max);
	void add(glm::vec3 v);
	void clear();
	const glm::vec3* getBounds() const {return &bounds[0];}
	glm::vec3 getCenter() const;
	bool overlaps(const AABB& other);
	/*
	 * 8 mat4 projections and an AABB copy
	 */
	AABB apply(const glm::mat4& m);

private:
	glm::vec3 bounds[2]; // {min, max}
};

typedef enum OctreeFlagBits {
	OCTREE_FLAG_BITS_NONE = 0x00,
	OCTREE_FLAG_BITS_ALL_CULLED = 0x01,
	OCTREE_FLAG_BITS_NONE_CULLED = 0x02,
} OctreeFlagBits;
typedef uint8_t OctreeFlags;

class Octree {
public:
	Octree() : aabb(), depth(0), children(nullptr), flags(OCTREE_FLAG_BITS_NONE) {}
	Octree(const Octree& lvalue);
	Octree(Octree&& rvalue);
	Octree(const std::vector<Mesh*> m, const std::vector<InstancedMesh*> im, uint8_t d);
	~Octree();
	Octree& operator=(Octree rhs);

	friend void swap(Octree& lhs, Octree& rhs);

	void frustumCull(const glm::mat4& v, const glm::mat4& p, std::map<const MeshBase*, bool>& cull_map);

private:
	AABB aabb;
	uint8_t depth;
	Octree* children;
	std::vector<Mesh*> meshes;
	std::vector<InstancedMesh*> inst_meshes;
	std::vector<std::vector<size_t>> inst_idxs;
	OctreeFlags flags;

	Octree(const AABB& a, const std::vector<Mesh*> m, const std::vector<InstancedMesh*> im, uint8_t d);

	void calculateChildren();
	/*
	 * Calculates whether this octree node collides with a given frustum matrix
	 * using the separating axis theorem in view space.
	 */
	bool intersectsFrust(const glm::mat4& v, const glm::mat4& p);
	bool containedByFrust(const glm::mat4& v, const glm::mat4& p);
	void cullAll(std::map<const MeshBase*, bool>& cull_map);
	void cullNone(std::map<const MeshBase*, bool>& cull_map);

	/*
	 * a is pre-normalized axis to test against in view space
	 *
	 * returns: true if they DO NOT INTERSECT, false otherwise
	 */
	bool axisTest(glm::vec3 a, const glm::mat4& v, const glm::mat4& p_inv);
};
#endif

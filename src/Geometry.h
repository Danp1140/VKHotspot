#include "Projection.h"

class AABB {
	AABB() : bounds({glm::vec3(std::numeric_limits<float>::infinity()), -glm::vec3(std::numeric_limits<float>::infinity())}) {}
	AABB(glm::vec3 min, glm::vec3 max) : bounds({min, max}) {}
	void add(glm::vec3 v);
	void clear();
	const glm::vec3* getBounds() const {return &bounds[0];}
	glm::vec3 getCenter() const;
	bool overlaps(const AABB& other);
private:
	glm::vec3 bounds[2]; // {min, max}
}

class Octree {
public:
	Octree(const std::vector<const MeshBase*> meshes, uint8_t d);
	Octree(const AABB& a, const std::vector<const MeshBase*> meshes, uint8_t d);

	void frustumCull(const glm::mat4& f);

private:
	AABB aabb;
	uint8_t depth;
	Octree children[8];

	void calculateChildren();
	bool intersectsFrust(const glm::mat4& f);
};

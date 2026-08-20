#ifndef GEOMETRY_H
#define GEOMETRY_H

class AABB;
class Octree;

#include "Projection.h"
#include "Mesh.h"

template<uint8_t D, typename T=float>
class Vec {
public:
	Vec() = default;
	Vec(T v) {
		for (uint8_t d = 0; d < D; d++) 
			data[d] = (T)v;
	}

	T& operator[](const uint8_t i) {
		if (i > D) 
			FatalError("Index " + std::to_string(i) + " out of range for vector of dimension " + std::to_string(D)).raise();
		return data[i];
	}
	Vec<D, T> operator+(const Vec<D, T>& rhs) const {
		Vec<D, T> res;
		for (uint8_t d = 0; d < D; d++) 
			res.data[d] = data[d] + rhs.data[d];
		return res;
	}
	Vec<D, T> operator-(const Vec<D, T>& rhs) const {
		Vec<D, T> res;
		for (uint8_t d = 0; d < D; d++) 
			res.data[d] = data[d] - rhs.data[d];
		return res;
	}
	Vec<D, T> operator*(const T& rhs) const {
		Vec<D, T> res;
		for (uint8_t d = 0; d < D; d++)
			res.data[d] = data[d] * rhs;
		return res;
	}
	Vec<D, T> operator/(const T& rhs) const {
		Vec<D, T> res;
		for (uint8_t d = 0; d < D; d++)
			res.data[d] = data[d] / rhs;
		return res;
	}
	const Vec<D, T>& operator-=(const Vec<D, T>& rhs) {
		*this = *this - rhs;
		return *this;
	}
	T magSq() const {
		T res = (T)0;
		for (uint8_t d = 0; d < D; d++) 
			res += pow(data[d], 2);
		return res;
	}
	T mag() const {
		return sqrt(magSq());
	}

private:
	T data[D];
};

template<uint8_t D_col, uint8_t D_row, typename T=float>
class Mat {
public:
private:
	Vec<D_row> cols[D_col];
};

template<uint8_t D, typename T=float>
class Facet {
public:
	Facet(Vec<D, T>** v) {
		for (uint8_t d = 0; d < D+1; d++) {
			member_vertices[d] = v[d];
			direct_adj[d] = nullptr;
		}
	}
	Facet(Facet<D-1, T>* f, Vec<D, T>* v) {
		for (uint8_t d = 0; d < D; d++) {
			member_vertices[d] = f.member_vertices[d];
			direct_adj[d] = nullptr;
		}
		member_vertices[D] = v;
		direct_adj[D] = nullptr;
	}

	size_t operator[](const uint8_t i) const {
		if (i > D) 
			FatalError("Index " + std::to_string(i) + " out of range for facet of dimension " + std::to_string(D)).raise();
		return indices[i];
	}

	std::set<Facet<D-1, T>> getChildFacets() const {
		std::set<Facet<D-1, T>> res;
		Vec<D, T> vert_tmp[D];
		for (uint8_t d = 0; d < D; d++) {
			/*
			 * just do a scrolling window of len D-1 that can wrap around
			 * so for a tetrahedron, indices[0, 1, 2], [1, 2, 3], [2, 3, 0], [3, 0, 1]
			 */
			for (uint8_t i = 0; i < D - 1; i++) 
				vert_tmp[i] = member_vertices[(d + i) % D];
			res.insert(Facet<D-1, T>(vert_tmp));
		}
	}
	Vec<D, T>* getUnique(const Facet<D, T>& other) { // returns this's member_vertex not contained in other, both dD facets 
		bool found;
		for (uint8_t d1 = 0; d1 < D; d1++) {
			found = false;
			for (uint8_t d2 = 0; d2 < D; d2++) {
				if (member_vertices[d1] == other.member_vertices[d2]) {
					found = true;
					break;
				}
			}
			if (!found) return member_vertices[d1]
		}
		return nullptr;
	}
	Vec<D, T> centroid() {
		Vec<D, T> res;
		for (uint8_t i = 0; i < D+1; i++) 
			res += member_vertices[d];
		return res / (T)(D+1);
	}

private:
	Vec<D, T>* member_vertices[D+1];
	Facet<D, T>* direct_adj[D+1]; // facets of dim D which are adjacent via a facet of dim D-1
																// a nullptr means that face is unbounded 
};

template<uint8_t D, typename T=float>
class Graph {
public:
	~Graph() {
		for (Vec<D, T>* v : vertices) delete v;
		for (Facet<D, T>* f : facets) delete f;
	}
protected:
	std::vector<Vec<D, T>*> vertices;
	
	void removeFacet(Facet f) {
		Facet* adj;
		for (uint8_t d = 0; d < f.getDim() + 1; d++) {
			adj = f.getDirAdj()[d];
			if (adj)
				adj.replaceDirAdj(f, nullptr);
		}
		facets[f.getDim() - 2].remove(f);
	}

private:
	std::set<Facet<D, T>*> facets; // Dd facets. lower dim facets are implicit 
};

template<uint8_t D, typename T=float>
class DelaunayGraph : public Graph<D, T> {
	using Graph<D, T>::vertices;
	using Graph<D, T>::facets;
public:
	/* 
	 * by default constructs a hypertetrahedron with each side length 1 centered on origin
	 */
	DelaunayGraph() {
		vertices.push_back(Vec<D, T>(0));
		Vec<D, T> to_add, centroid(0);
		for (uint8_t d = 0; d < D; d++) {
			to_add = centroid;
			to_add[d] = sqrt(1 - centroid.magSq());
			vertices.push_back(new Vec<D, T>(to_add));
			centroid = (centroid*(d+1) + to_add) / (d+2);
		}
		Vec<D, T>* Dd_facets_temp[D+1];
		for (uint8_t d = 0; d < D; d++) {
			*vertices[d] -= centroid;
			Dd_facets_temp[d] = verctices[d];
		}
		facets.insert(new Facet<D, T>(Dd_facets_temp));
	}

	/* 
	 * places a vertex at the centroid of the D-dimensional facet at the given index
	 */
	void addVertex(const Facet* f) {
		Vec<D, T>* to_add = new Vec<D, T>(f->centroid());

		Facet<D, T>* adj;
		for (uint8_t d = 0; d < D + 1; d++) {
			std::set<Facet<D, T>*> cavity;
			adj = f->getDirAdj()[d];
			if (adj) digCavity(to_add, adj, f->getUnique(*adj), cavity);
			fillCavity();
		}

		facets.remove(f);
		delete f;

		// handle delaunay nastiness
		//
		// remove the given Dd facet
		// make new facets between vertex and removed Dd facet's D+1 (D-1)d child facets
		// for each created facet:
		//   find the Dd facet it is adjacent to by the shared (D-1)d child facet
		//   check if non shared vertex is in circumsphere
		//     if not all good
		//     if so, create a cavity by destroying both Dd facets
		//       on the D old Dd facets adjacent to the cavity just created, run the same test against the newest vertex
		//       continue digging the cavity until all good
		//       fill the cavity
		//         for each (D-1)d facet of the cavity NOT adjacent to the newest vertex, create a Dd facet between it
		//         and the newest vertex
		//
		// TODOs for this algorithm:
		// - easy addition and removal of arbitrary dimension facets (namely D and D-1 dimensional)
		// - easy determination of adjacent facets
		// - easy comparison of if facets are same
		// - determination of which vertex is part of parent facet but not child
		// - circumsphere test
	}
private:
	/*
	 * v is the added vertex
	 * f is the Dd facet to test against
	 * w is the vertex of f that is not on the cavity boundary (undergoes circumcenter test against Dd facet between v and
	 * remaining vertices of f)
	 * boundary is a set of (D-d) facets CONFIRMED to be part of the boundary
	 */
	void digCavity(Vec<D, T>* v, Facet<D, T>* f, Vec<D, T>* w, std::set<Facet>& boundary) {
		Facet<D-1, T> poss_bound = f->getNot(w);
		float c = Facet(poss_bound, v).circumcenter();
		if (c > 0) {
			boundary.insert(poss_bound);
		}
		else if (c < 0) {
			facets[D-1].remove(f);
			Facet* next;
			for (uint8_t d = 0; d < D+1; d++) {
				next = f.getDirAdj()[d];
				if (next) 
					digCavity(v, *next, next.getUnique(f), boundary);
			}
			facets.remove(f);
			delete f;
		}
		else {
			FatalError("Circumcenter test exactly 0, unimplemented").raise();
		}
	}
	/*
	 */
	void fillCavity(Facet<D, T>* v_adj_f, std::set<Facet<D, T>*>& boundary) {
		
	}
};

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
	AABB applyHomo(const glm::mat4& m);

private:
	glm::vec3 bounds[2]; // {min, max}
};

typedef enum OctreeFlagBits {
	OCTREE_FLAG_BITS_NONE = 0x00,
	OCTREE_FLAG_BITS_ALL_CULLED = 0x01,
	OCTREE_FLAG_BITS_NONE_CULLED = 0x02,
	OCTREE_FLAG_BITS_CHEAP = 0x04
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
	void setCheap();

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
	 * TODO: see what we can make more efficient, this is a big check
	 */
	bool intersectsFrust(const glm::mat4& v, const glm::mat4& p);
	bool containedByFrust(const glm::mat4& v, const glm::mat4& p);
	/*
	 * Calculates whether this octree nodes collides with a given frustum matrix
	 * using an AABB around the frustum. This may lead to false positives but will
	 * never lead to a false negative (and is MUCH faster)
	 */
	bool cheapIntersectsFrust(const glm::mat4& v, const glm::mat4& p);

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

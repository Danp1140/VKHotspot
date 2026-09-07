#ifndef GEOMETRY_H
#define GEOMETRY_H

class AABB;
class Octree;

#include "Projection.h"
#include "Mesh.h"
#include <sstream>

// row major
// D_row is number of entries in row, num col
// D_col is number of entries in col, num row
template<uint8_t D_row, uint8_t D_col, typename T=float>
class Mat {
public:
	Mat() {
		data = new T[D_row * D_col];
		memset(data, 0, D_row * D_col * sizeof(T));
	}
	Mat(const Mat& lvalue) : Mat() {
		memcpy(data, lvalue.data, D_row*D_col*sizeof(T));
	}
	Mat(Mat&& rvalue) : Mat() {
		data = rvalue.data;
		rvalue.data = nullptr;
	}
	Mat(uint8_t r, uint8_t c) : Mat() {
		data[c*D_row + r] = 1;
	}
	Mat(const T* d) : Mat() {
		memcpy(data, d, D_row * D_col * sizeof(T));
	}
	Mat(const T* d, size_t stride) : Mat() {
		for (uint8_t i = 0; i < D_row; i++) {
			for (uint8_t j = 0; j < D_col; j++) {
				data[i * D_col + j] = d[(i * D_col + j) * stride];
			}
		}
	}
	~Mat() {
		if (data) delete[] data;
	}
	static Mat I() {
		Mat I;
		for (uint8_t d = 0; d < D_row; d++) I.data[d*D_row + d] = 1;
		return I;
	}

	friend void swap(Mat& lhs, Mat& rhs) {
		std::swap(lhs.data, rhs.data);
	}

	Mat<D_row, 1, T> getRow(uint8_t n) const {
		return Mat<D_row, 1, T>(data + n * D_row);
	}
	Mat<1, D_col, T> getCol(uint8_t n) const {
		return Mat<1, D_col, T>(data + n, D_row);
	}
	void setRow(uint8_t n, Mat<D_row, 1, T> r) {
		memcpy(data + n*D_row, r.data, D_row*sizeof(T));
	}
	void setCol(uint8_t n, Mat<1, D_col, T> c) {
		for (uint8_t r = 0; r < D_row; r++) {
			data[r*D_row + n] = c.data[r];
		}
	}

	Mat<D_col, D_row, T> trans() const {
		Mat<D_col, D_row, T> M;
		for (uint8_t r = 0; r < D_row; r++) {
			for (uint8_t c = 0; c < D_col; c++) {
				M.data[r*D_col + c] = data[c*D_row + r];
			}
		}
		return M;
	}

	template<uint8_t rhs_D_row>
	Mat<rhs_D_row, D_col, T> operator*(const Mat<rhs_D_row, D_row, T>& rhs) {
		Mat<rhs_D_row, D_col, T> M;		
		for (uint8_t r = 0; r < rhs_D_row; r++) {
			for (uint8_t c = 0; c < D_col; c++) {
				M.data[c*rhs_D_row + r] = 0;
				for (uint8_t k = 0; k < D_row; k++) 
					M.data[c*rhs_D_row + r] += data[c*D_row + k] * rhs.data[k*rhs_D_row + r];
			}
		}
		return M;
	}
	Mat<D_row, D_col, T> operator/(const T& rhs) const {
		Mat<D_row, D_col, T> M;
		for (uint8_t r = 0; r < D_row; r++) {
			for (uint8_t c = 0; c < D_col; c++) {
				M.data[r * D_col + c] = data[r * D_col + c] / rhs;
			}
		}
		return M;
	}
	Mat<D_row, D_col, T>& operator=(Mat rhs) {
		swap(*this, rhs);
		return *this;
	}	

	void LUQ(Mat<D_row, D_col, T>& L, Mat<D_row, D_col, T>& U, Mat<D_row, D_col, T>& Q) {
		if constexpr (D_row == 1 || D_col == 1) {
			L.data[0] = 1;
			U.data[0] = data[0];
			Q.data[0] = 1;
		}
		else {
			/*
			 * Finding pivot
			 */
			uint8_t max_piv_col_idx = 0;
			T max_piv_val = abs(data[0]);
			for (uint8_t p = 1; p < D_row; p++) {
				if (abs(data[p]) > max_piv_val) {
					max_piv_val = abs(data[p]);
					max_piv_col_idx = p;
				}
			}
			if (max_piv_val == (T)0)
				FatalError("Uh oh top row of input mat was all 0").raise();
			Q.setCol(0, Mat<1, D_col, T>((uint8_t)0, max_piv_col_idx));
			for (uint8_t r = 1; r < D_row; r++) {
				if (r <= max_piv_col_idx)
					Q.setCol(r, Mat<1, D_col, T>((uint8_t)0, r-1));
				else if (r > max_piv_col_idx)
					Q.setCol(r, Mat<1, D_col, T>((uint8_t)0, r));
			}
			Mat<D_row, D_col, T> pivoted = *this * Q;

			/*
			 * Setting known parts of U and L
			 */
			U.setCol(0, Mat<1, D_col, T>((uint8_t)0, (uint8_t)0)); // left col of U is all 0 except top left, which is overwritten next line
			U.setRow(0, pivoted.getRow(0));                        // top row of U is same as that of *this
			L.setCol(0, pivoted.getCol(0) / pivoted.data[0]);      // left col of L is that of *this div by upper left of *this
			L.setRow(0, Mat<D_row, 1, T>((uint8_t)0, (uint8_t)0)); // top row of L is just 1 at the left 

			/*
			 * Next recursive call
			 */
			Mat<1, D_col-1, T> l;
			memcpy(l.data, &pivoted.getCol(0).data[1], (D_col-1)*sizeof(T));
			l = l / pivoted.data[0];
			Mat<D_row-1, 1, T> uT;
			memcpy(uT.data, &pivoted.getRow(0).data[1], (D_row-1)*sizeof(T));
			Mat<D_row-1, D_col-1, T> next_M, next_L, next_U, next_Q, outer_prod = l * uT;

			for (uint8_t r = 0; r < D_row - 1; r++) {
				for (uint8_t c = 0; c < D_col - 1; c++) {
					next_M.data[c*(D_row-1) + r] = pivoted.data[(c+1)*D_row+r+1] - outer_prod.data[c*(D_row-1) + r];
				}
			}
			std::cout << "l:" << std::endl;
			std::cout << l.to_string() << std::endl;
			std::cout << "uT:" << std::endl;
			std::cout << uT.to_string() << std::endl;
			std::cout << "pivoted A:" << std::endl;
			std::cout << pivoted.to_string() << std::endl;
			std::cout << "luT:" << std::endl;
			std::cout << outer_prod.to_string() << std::endl;

			next_M.LUQ(next_L, next_U, next_Q);

			/*
			 * Pull out data
			 */
			Mat<D_row, D_col, T> temp_Q;
			temp_Q.data[0] = 1;
			for (uint8_t r = 0; r < D_row - 1; r++) {
				for (uint8_t c = 0; c < D_col - 1; c++) {
					L.data[(c+1)*D_row+r+1] = next_L.data[c*(D_row-1) + r];
					U.data[(c+1)*D_row+r+1] = next_U.data[c*(D_row-1) + r];
					temp_Q.data[(c+1)*D_row+r+1] = next_Q.data[c*(D_row-1) + r];
				}
			}
			Q = Q * temp_Q;
		}
	}

	Mat<D_row, D_col, T> invert() {
		Mat<D_row, D_col, T> L, U, Q;
		LUQ(L, U, Q);
		for (uint8_t c = 0; c < D_col; c++) {

		}
	}

	std::string to_string() const {
		std::ostringstream ss;
		ss << "┌";
		ss << std::string(D_row * 7, ' ');
		ss << "┐\n";
		for (uint8_t c = 0; c < D_col; c++) {
			ss << "|";
			for (uint8_t r = 0; r < D_row; r++) {
				ss << std::format("{:>4.3f}", data[c * D_row + r]);
				ss << " ";
			}	
			ss << "|\n";
		}
		ss << "└";
		ss << std::string(D_row * 7, ' ');
		ss << "┘";
		return ss.str();
	}

	T* data;

private:

};

// typedef Mat<D, 1, T> Vec<D, T>

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
			member_vertices[d] = f->member_vertices[d];
			direct_adj[d] = nullptr;
		}
		member_vertices[D] = v;
		direct_adj[D] = nullptr;
	}

	Vec<D, T>* operator[](const uint8_t i) const {
		if (i > D + 1) 
			FatalError("Index " + std::to_string(i) + " out of range for facet of dimension " + std::to_string(D)).raise();
		return member_vertices[i];
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
			if (!found) return member_vertices[d1];
		}
		return nullptr;
	}
	Vec<D, T> centroid() {
		Vec<D, T> res;
		for (uint8_t d = 0; d < D+1; d++) 
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

	void addFacet(Facet<D, T>* f) {facets.insert(f);}
	void removeFacet(Facet<D, T>* f) {facets.remove(f);}
protected:
	std::vector<Vec<D, T>*> vertices;
	
/*
	void removeFacet(Facet f) {
		Facet* adj;
		for (uint8_t d = 0; d < f.getDim() + 1; d++) {
			adj = f.getDirAdj()[d];
			if (adj)
				adj.replaceDirAdj(f, nullptr);
		}
		facets[f.getDim() - 2].remove(f);
	}
*/

private:
	std::set<Facet<D, T>*> facets; // Dd facets. lower dim facets are implicit 
};

template<uint8_t D, typename T=float>
class DelaunayGraph : public Graph<D, T> {
	using Graph<D, T>::vertices;
	// using Graph<D, T>::facets;
public:
	/* 
	 * by default constructs a hypertetrahedron with each side length 1 centered on origin
	 */
	DelaunayGraph() {
		vertices.push_back(new Vec<D, T>(0));
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
			Dd_facets_temp[d] = vertices[d];
		}
		Graph<D, T>::addFacet(new Facet<D, T>(Dd_facets_temp));
	}

	/* 
	 * places a vertex at the centroid of the D-dimensional facet at the given index
	 */
	void addVertex(const Facet<D, T>* f) {
		Vec<D, T>* to_add = new Vec<D, T>(f->centroid());

		Facet<D, T>* adj;
		for (uint8_t d = 0; d < D + 1; d++) {
			std::set<Facet<D, T>*> cavity;
			adj = f->getDirAdj()[d];
			if (adj) digCavity(to_add, adj, f->getUnique(*adj), cavity);
			fillCavity();
		}

		removeFacet(f);
		delete f;
	}
private:
	/*
	 * v is the added vertex
	 * f is the Dd facet to test against
	 * w is the vertex of f that is not on the cavity boundary (undergoes circumcenter test against Dd facet between v and
	 * remaining vertices of f)
	 * boundary is a set of (D-d) facets CONFIRMED to be part of the boundary
	 */
	void digCavity(Vec<D, T>* v, Facet<D, T>* f, Vec<D, T>* w, std::set<Facet<D-1, T>>& boundary) {
		Facet<D-1, T> poss_bound = f->getNot(w);
		float c = Facet(poss_bound, v).circumcenter();
		if (c > 0) 
			boundary.insert(poss_bound);
		else if (c < 0) {
			Facet<D, T>* next;
			for (uint8_t d = 0; d < D+1; d++) {
				next = f->getDirAdj()[d];
				if (next) 
					digCavity(v, *next, next->getUnique(f), boundary);
			}
			removeFacet(f);
			delete f;
		}
		else {
			FatalError("Circumcenter test exactly 0, unimplemented").raise();
		}
	}
	/*
	 */
	void fillCavity(std::set<Facet<D-1, T>*>& v_adj_f, std::set<Facet<D-1, T>*>& boundary) {
		if (boundary.size() == 1) {
			addFacet(new Facet<D, T>(v_adj_f));
		}
		else {
			Facet<D-1, T>* f1 = *v_adj_f.begin(), * f2 = nullptr;
			for (uint8_t d = 0; d < D; d++) {
				if (boundary.contains(f1->getDirAdj()[d]) && !v_adj_f.contains(f1->getDirAdj()[d])) {
					f2 = f1->getDirAdj()[d];
					break;
				}
			}
			if (f2 == nullptr) 
				FatalError("Couldn't find second facet for fillCav").raise();
			addFacet(new Facet<D, T>(f1, f2));
			boundary.remove(f2);
			v_adj_f.remove(f1);
			// v_adj_f.add(
			fillCavity(v_adj_f, boundary);
		}
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

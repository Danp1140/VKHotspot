#ifndef GEOMETRY_H
#define GEOMETRY_H

// #define GRAPH_TROUBLESHOOT

class AABB;
class Octree;

#include "Projection.h"
#include "Mesh.h"
#include <sstream>

typedef uint8_t Dim;

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
	
	T get(Dim row, Dim col) const {return data[col*D_row + row];}
	// wraps OOB values by row and col individually
	T getWrap(Dim row, Dim col) const {return data[(col%D_col)*D_row + (row%D_row)];}
	Mat<D_row, 1, T> getRow(Dim n) const {return Mat<D_row, 1, T>(data + n * D_row);}
	Mat<1, D_col, T> getCol(Dim n) const {return Mat<1, D_col, T>(data + n, D_row);}
	void setRow(Dim n, Mat<D_row, 1, T> r) {memcpy(data + n*D_row, r.data, D_row*sizeof(T));}
	void setCol(Dim n, Mat<1, D_col, T> c) {
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
	T determinant() const {
		T res = 0;
		if constexpr (D_row == 1) res = data[0];
		else if constexpr (D_row == 2) res = data[0] * data[3] - data[1] * data[2];
		else {
			Mat<D_row - 1, D_col - 1, T> M;
			for (Dim d = 0; d < D_row; d++) {
				Dim row_offset = 0;
				for (Dim row = 0; row < D_row; row++) {
					if (row == d) {
						row_offset++;
					}
					else {
						for (Dim col = 0; col < D_col - 1; col++) {
							M.data[col*(D_row-1) + row - row_offset] = getWrap(row, col + 1);
						}
					}
				}
				res += data[d] * M.determinant() * pow(-1, d);
			}
		}
		return res;
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
	Mat<D_row, D_col, T> operator*(const T& rhs) const {
		Mat<D_row, D_col, T> M;
		for (uint8_t r = 0; r < D_row; r++) {
			for (uint8_t c = 0; c < D_col; c++) {
				M.data[r * D_col + c] = data[r * D_col + c] * rhs;
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

	Mat<D_row, D_col, T> mirrorFromUpper() {
		Mat<D_row, D_col, T> res = *this;
		for (Dim r = 0; r < D_row; r++) {
			for (Dim c = r+1; c < D_col; c++ ) {
				res.data[c*D_row + r] = data[r*D_col + c];
			}
		}
		return res;
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
					/*
					if (r < max_piv_col_idx) 
						Q.data[(c+1)*D_row+r] = next_Q.data[c*(D_row-1)+r];
					else if (r >= max_piv_col_idx)
						Q.data[(c+1)*D_row+(r+1)] = next_Q.data[c*(D_row-1)+r];
					*/
				}
			}

			/*
			 * Setting known parts of U and L
			 */
			Q = Q * temp_Q;
			uT = uT * next_Q;
			Mat<D_row, 1, T> uT_long;
			uT_long.data[0] = pivoted.data[0];
			memcpy(&uT_long.data[1], uT.data, (D_row - 1)*sizeof(T));
			U.setCol(0, Mat<1, D_col, T>((uint8_t)0, (uint8_t)0)); // left col of U is all 0 except top left, which is overwritten next line
			U.setRow(0, uT_long);                        // top row of U is same as that of *this
			L.setCol(0, pivoted.getCol(0) / pivoted.data[0]);      // left col of L is that of *this div by upper left of *this
			L.setRow(0, Mat<D_row, 1, T>((uint8_t)0, (uint8_t)0)); // top row of L is just 1 at the left 
		}
	}

	Mat<D_row, D_col, T> invert() {
		Mat<D_row, D_col, T> L, U, Q, M;
		LUQ(L, U, Q);
		Mat<1, D_col, T> w, v;
		for (uint8_t r = 0; r < D_row; r++) {
			for (uint8_t c = 0; c < D_col; c++) {
				w.data[c] = (c == r ? 1 : 0);
				for (uint8_t c2 = 0; c2 < c; c2++) 
					w.data[c] -= w.data[c2] * L.data[c*D_row + c2];
				w.data[c] /= L.data[c*D_row + c];
			}
			for (uint8_t c = D_col - 1; c < (uint8_t)-1; c--) {
				v.data[c] = w.data[c];
				for (uint8_t c2 = c+1; c2 < D_col; c2++) 
					v.data[c] -= v.data[c2] * U.data[c*D_row + c2];
				v.data[c] /= U.data[c*D_row + c];
			}
			M.setCol(r, v);
		}
		return Q * M;
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
	Vec() {
		for (uint8_t d = 0; d < D; d++) 
			data[d] = (T)0;
	}
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
	T dot(const Vec<D, T>& rhs) const {
		T res = 0;
		for (Dim d = 0; d < D; d++) res += data[d] * rhs.data[d];
		return res;
	}

	std::string to_string() {
		std::string res = "[";
		for (Dim d = 0; d < D; d++) 
			res += std::to_string(data[d]) + ", ";
		res.pop_back();
		res += "]";
		return res;
	}

private:
	T data[D];
};

// N-simplex existing in dimension D with positional precision T
template<Dim D, Dim N, typename T=float>
class Simplex {
public:
	Simplex() : parent(nullptr) {}
	Simplex(Vec<D, T>** v) : parent(nullptr) {
		for (Dim n = 0; n < N; n++) {
			member_vertices[n] = v[n];
			direct_adj[n] = nullptr;
		}
	}
	// does no adjacency setting, not enough known
	Simplex(Simplex<D, N-1, T>* f, Vec<D, T>* v) : parent(nullptr) {
		for (Dim n = 0; n < N - 1; n++) {
			member_vertices[n] = f->getMemberVertex(n);
			direct_adj[n] = nullptr;
		}
		member_vertices[N - 1] = v;
		direct_adj[N - 1] = nullptr;
	}
	// does no adjacency setting, not enough known
	Simplex(std::set<Simplex<D, N-1, T>>& in) : parent(nullptr) {
		std::set<Vec<D, T>*> verts;
		for (const Simplex<D, N-1, T>& s : in) {
			for (Dim n = 0; n < N - 1; n++) verts.insert(s.getMemberVertex(n));
		}
		if (verts.size() != N) FatalError("Simplex init using set must have " + std::to_string(N) + " unique vertices, given have " + std::to_string(verts.size())).raise();
		Dim i = 0;
		for (Vec<D, T>* v : verts) {
			member_vertices[i] = v;
			i++;
		}
	}

	Vec<D, T>* operator[](Dim n) const {
		if (n > N) {
			FatalError("Index " + std::to_string(n) + " out of range for " + std::to_string(N) + " facet").raise();
			return nullptr;
		}
		return member_vertices[n];
	}
	bool operator<(const Simplex<D, N, T>& rhs) const {
		for (Dim n = 0; n < N; n++) {
			if (member_vertices[n] < rhs.member_vertices[n]) return true;
			else if (member_vertices[n] > rhs.member_vertices[n]) return false;
		}
		return false;
	}
	bool operator>(const Simplex<D, N, T>& rhs) const {
		for (Dim n = 0; n < N; n++) {
			if (member_vertices[n] > rhs.member_vertices[n]) return true;
			else if (member_vertices[n] < rhs.member_vertices[n]) return false;
		}
		return false;
	}

	void setParent(Simplex<D, N+1, T>* p) {parent = p;}
	void setDirAdj(Dim n, Simplex<D, N, T>* s) {direct_adj[n] = s;}

	Simplex<D, N+1, T>* getParent() {return parent;}
	Vec<D, T>* getMemberVertex(Dim n) const {return member_vertices[n];}
	Simplex<D, N, T>** getDirAdj() {return &direct_adj[0];}
	Simplex<D, N, T>* const * getDirAdj() const {return &direct_adj[0];}
	// creates and returns N-1-simplex child that is shared with N-simplex directly adjacent at index n
	Simplex<D, N-1, T> getDirAdj(Dim n) {
		Vec<D, T>* points[N-1];
		if (!direct_adj[n]) {
			bool someone_is_missing[N];
			memset(&someone_is_missing[0], 0, sizeof(bool) * N);
			for (Dim n_i = 0; n_i < N; n_i++) {
				if (n_i != n) {
					if (!direct_adj[n]) FatalError("Cannot getDirAdj(Dim n) on simplex with more than one null adjacency").raise();
					for (Dim n_i2 = 0; n_i2 < N; n_i2++) {
						if (!someone_is_missing[n_i2] && !direct_adj[n]->contains(member_vertices[n_i2])) someone_is_missing[n_i2] = true;
					}
				}
			}
			Dim counter = 0;
			for (Dim n_i = 0; n_i < N; n_i++) {
				if (someone_is_missing[n_i]) {
					points[counter] = member_vertices[n_i];
					counter++;
				}
			}	
		}
		else {
			Vec<D, T>* u = getUnique(*direct_adj[n]);
			Dim idx = 0;
			for (Dim n = 0; n < D+1; n++) {       // makes an N-1 simplex from all vertices that are not u, which is not contained in *this
				if (member_vertices[n] != u) {
					points[idx] = member_vertices[n];
					idx++;
				}
			}
		}
		Simplex<D, N-1, T> res(&points[0]);
		res.setParent(this);
		return res;
	}
	// sets s to appropriate dir adj, the one at the same index of the member vertex not contained in s
	void swapDirAdj(Simplex<D, N, T>* s) {
		bool found;
		for (Dim n1 = 0; n1 < N; n1++) {
			found = false;
			for (uint8_t n2 = 0; n2 < N; n2++) {
				if (member_vertices[n1] == s->member_vertices[n2]) {
					found = true;
					break;
				}
			}
			if (!found) {
				direct_adj[n1] = s;
				return;
			}
		}
		FatalError("Couldn't find vertex not shared with s in swapDirAdj").raise();
	}

	std::set<Simplex<D, N-1, T>> getChildren() {
		std::set<Simplex<D, N-1, T>> res;
		Simplex<D, N-1, T> tmp;
		Vec<D, T>* vert_tmp[N-1];
		for (Dim n = 0; n < N; n++) {
			// just do a scrolling window of len D-1 that can wrap around
			// so for a tetrahedron, indices[0, 1, 2], [1, 2, 3], [2, 3, 0], [3, 0, 1]
			for (uint8_t i = 0; i < N - 1; i++) 
				vert_tmp[i] = member_vertices[(n + i) % N];
			tmp = Simplex<D, N-1, T>(&vert_tmp[0]);
			tmp.setParent(this);
			res.insert(tmp);
		}
		return res;
	}
	// no parent assigned so it can remain const, but only geometric data
	Simplex<D, N-1, T> getTempChild(Dim n) const {
		Simplex<D, N-1, T> tmp;
		Vec<D, T>* vert_tmp[N-1];
		// just do a scrolling window of len D-1 that can wrap around
		// so for a tetrahedron, indices[0, 1, 2], [1, 2, 3], [2, 3, 0], [3, 0, 1]
		for (uint8_t i = 0; i < N - 1; i++) 
			vert_tmp[i] = member_vertices[(n + i) % N];
		tmp = Simplex<D, N-1, T>(&vert_tmp[0]);
		return tmp;
	}
	bool contains(const Vec<D, T>* v) const {
		for (Dim n = 0; n < N; n++) {
			if (member_vertices[n] == v) return true;
		}
		return false;
	}
	Vec<D, T>* getUnique(const Simplex<D, N, T>& other) const { // returns this's member_vertex not contained in other, both dD facets 
		bool found;
		for (Dim n1 = 0; n1 < N; n1++) {
			found = false;
			for (uint8_t n2 = 0; n2 < N; n2++) {
				if (member_vertices[n1] == other.member_vertices[n2]) {
					found = true;
					break;
				}
			}
			if (!found) return member_vertices[n1];
		}
		return nullptr;
	}
	// doesn't set parent to this so as to remain const
	Simplex<D, N-1, T> getNot(Vec<D, T>* v) const {
		Vec<D, T>* verts[N-1];
		Dim i = 0;
		for (Dim n = 0; n < N; n++) {
			if (member_vertices[n] != v) {
				verts[i] = member_vertices[n];
				i++;
			}
		}
		return Simplex<D, N-1, T>(&verts[0]);
	}
	void swapDirAdj(const Simplex<D, N, T>* old, Simplex<D, N, T>* newe) {
		for (Dim n = 0; n < N; n++) {
			if (direct_adj[n] == old) {
				direct_adj[n] = newe;
				return;
			}
		}
		FatalError("Didn't find direct adjacency to swap").raise();
	}
	Dim nOverlap(const Simplex<D, N, T>& s) const {
		Dim res = 0;
		for (Dim n1 = 0; n1 < N; n1++) {
			for (Dim n2 = n1; n2 < N; n2++) {
				if (member_vertices[n1] == s.getMemberVertex(n2)) res++;
			}
		}
		return res;
	}

	bool vecLiesIn(Vec<D, T>& v) const {
		Vec<D, T> d;
		Simplex<D, N-1, T> child;
		for (Dim n = 0; n < N; n++) {
			d = v - *member_vertices[n];
			child = getTempChild(n);
			if (child.sideTest(d) > 0) // if outside on any side
				return false;
		}
		return true;
	}
	// < 0 if on "in" side
	// 0 if lies on simplex's space (not bounded by vertices, just in the D-dim space the simplex lies in)
	// > 0 if on "out" side
	T sideTest(Vec<D, T>& v) const {
		Vec<D, T> n = norm();
		return v.dot(norm()); // inward facing normal
	}
	Vec<D, T> norm() const {
		// TODO: order here is crucial, combined with implicit info of member_vertices order
		// to ensure normal points outward
		// currently seems to work for 2-facets in 2D, testing insufficient
		
		// for this to result in a single vector instead of a space, it requires D = N
		Vec<D, T> diff_vecs[N - 1];
		for (Dim n = 0; n < N - 1; n++) diff_vecs[n] = *member_vertices[n + 1] - *member_vertices[0];
		Mat<N-1, N-1, T> M;
		Vec<D, T> res;
		for (Dim d = 0; d < D; d++) {
			Dim row_offset = 0;
			for (Dim row = 0; row < N; row++) {
				if (row == d) {
					row_offset++;
				}
				else {
					for (Dim col = 0; col < N - 1; col++) {
						M.data[col*(N-1) + row - row_offset] = diff_vecs[col][row];
					}
				}
			}
			res[d] = M.determinant() * pow(-1, d);
		}
		return res;
	}
	Vec<D, T> centroid() const {
		std::cout << "centroid call: " << std::endl;
		Vec<D, T> res;
		for (Dim n = 0; n < N; n++) {
			res = res + *member_vertices[n];
			std::cout << "adding " << member_vertices[n]->to_string() << std::endl;
		}
		return res / (T)N;
	}
	// < 0 if in circumsphere
	// 0 if on circumsphere
	// > 0 if out of circumsphere
	T circumcenterTest(Vec<D, T> v) const {
		for (Dim n = 0; n < N; n++) std::cout << member_vertices[n]->to_string() << std::endl;
		Mat<N+1, N+1, T> C;
		for (Dim n = 1; n < N+1; n++) {
			C.data[n] = 1;	
			C.data[n*(N+1)] = 1;
		}
		for (Dim r = 0; r < N; r++) {
			for (Dim c = 0; c < N; c++) {
				if (r > c)
					C.data[(c+1)*(N+1) + (r+1)] = (*member_vertices[c] - *member_vertices[r]).magSq();
			}
		}
		C = C.mirrorFromUpper();
		std::cout << "C-M mat: \n";
		std::cout << C.to_string() << std::endl;
		Mat<N+1, N+1, T> M = C.invert() * (T)(-2);
		T r = 0.5 * sqrt(M.data[0]);
		Vec<D, T> c;
		T denom = 0;
		for (Dim n = 1; n < N + 1; n++) {
			c = c + *member_vertices[n - 1] * M.data[n];
			denom += M.data[n];
		}
		c = c / denom;
		std::cout << "radius " << r << std::endl;
		std::cout << "center " << c.to_string() << std::endl;
		return (c - v).mag() - r;
	}

private:
	Vec<D, T>* member_vertices[N];
	Simplex<D, N, T>* direct_adj[N]; // N-simplices which are adjacent via a shared N-1-simplex
																// adj n is adjacent via an N-1 simplex of member_vertices v_n, ..., v_((n+N-1)%N)
																// for instance, with a triangle, adjacency 0 is adjacent via an edge of vertices 0 and 1
																// a nullptr means there is no adjacency on that simplex 
	Simplex<D, N+1, T>* parent; // nullptr if not part of a larger facet
};

// Graph in spatial dimension D made of N-simplices
template<Dim D, Dim N, typename T=float>
class Graph {
public:
	~Graph() {
		for (Vec<D, T>* v : vertices) delete v;
		for (Simplex<D, N, T>* s : simplices) delete s;
	}

	void addSimplex(Simplex<D, N, T>* s) {simplices.insert(s);}
	void removeSimplex(Simplex<D, N, T>* s) {
		#ifdef GRAPH_TROUBLESHOOT
		for (Simplex<D, N, T>* s_i : simplices) {
			for (Dim n = 0; n < N; n++) {
				if (s_i == s) s->setDirAdj(n, nullptr);
				else if (s_i->getDirAdj()[n] == s) FatalError("Deleting simplex that still has adjacency references").raise();
			}
		}
		#endif
		simplices.erase(s);
		// delete s;
	}
	const std::set<Simplex<D, N, T>*>& getSimplices() {return simplices;}

	size_t getNumVertices() const {return vertices.size();}
	size_t getNumSimplices() const {return simplices.size();}
	const std::vector<Vec<D, T>*>& getVertices() const {return vertices;}
	const std::set<Simplex<D, N, T>*>& getSimplices() const {return simplices;}

protected:
	std::vector<Vec<D, T>*> vertices;
	
private:
	std::set<Simplex<D, N, T>*> simplices; // Dd facets. lower dim facets are implicit 
};

template<Dim D, Dim N, typename T=float>
class DelaunayGraph : public Graph<D, N, T> {
	using Graph<D, N, T>::vertices;
public:
	/* 
	 * by default constructs a hypertetrahedron with each side length 1 centered on origin
	 */
	DelaunayGraph() {
		vertices.push_back(new Vec<D, T>(0));
		Vec<D, T> to_add, centroid(0);
		for (uint8_t n = 0; n < N-1; n++) {
			to_add = centroid;
			to_add[n] = sqrt(1 - centroid.magSq());
			vertices.push_back(new Vec<D, T>(to_add));
			centroid = (centroid*(n+1) + to_add) / (n+2);
		}
		Vec<D, T>* Dd_facets_temp[N];
		for (uint8_t n = 0; n < N; n++) {
			*vertices[n] -= centroid;
			Dd_facets_temp[n] = vertices[n];
		}
		Graph<D, N, T>::addSimplex(new Simplex<D, N, T>(Dd_facets_temp));
	}

	/* 
	 * places a vertex at the centroid of the D-dimensional facet at the given index
	 */
	void addVertex(Simplex<D, N, T>* f) {
		_addVertex(f, new Vec<D, T>(f->centroid()));	
	}
	void addVertex(Vec<D, T>& v) {
		// TODO: better algs exist, this is just to test
		Simplex<D, N, T>* s = nullptr;
		for (Simplex<D, N, T>* s_i : Graph<D, N, T>::getSimplices()) {
			if (s_i->vecLiesIn(v)) {
				s = s_i;
				break;
			}
		}
		if (!s) FatalError("Couldn't find simplex containing vector, adding exterior vertex not yet supported").raise();
		_addVertex(s, new Vec<D, T>(v));
	}
private:
	// vert_to_add should be alloc'd with new, graph will handle freeing
	// contains_vert should already be a valid simplex in the graph
	void _addVertex(Simplex<D, N, T>* contains_vert, Vec<D, T>* vert_to_add) {
		vertices.push_back(vert_to_add);
	
		Vec<D, T>* tmp[N];																																	// pre-construct new simplices for adjacency
		tmp[N-1] = vert_to_add;
		Simplex<D, N, T>* init_simps[N];
		for (Dim n = 0; n < N; n++) {
			for (Dim n2 = 0; n2 < N-1; n2++) 
				tmp[n2] = contains_vert->getMemberVertex((n + n2) % N);
			init_simps[n] = new Simplex<D, N, T>(&tmp[0]);
			Graph<D, N, T>::addSimplex(init_simps[n]);
		}
		for (Dim n = 0; n < N; n++) {																												// set adjacencies
			init_simps[n]->setDirAdj(N-1, contains_vert->getDirAdj()[(n - 1 + N) % N]);
			if (contains_vert->getDirAdj()[(n - 1 + N) % N]) contains_vert->getDirAdj()[(n - 1 + N) % N]->swapDirAdj(contains_vert, init_simps[n]);
			for (Dim n2 = 0; n2 < N-1; n2++) 
				init_simps[n]->setDirAdj(n2, init_simps[(n + n2 + 1) % N]);
		}

		for (Dim n = 0; n < N; n++) {
			std::set<Simplex<D, N-1, T>> cavity;
			Simplex<D, N, T>* to_dig = init_simps[n]->getDirAdj()[N-1];
			std::set<Simplex<D, N-1, T>> vta_adj;
			for (Dim adj_i = 0; adj_i < N-1; adj_i++) { // iterate through adj to other init_simps
				Simplex<D, N-1, T> vta_adj_temp = init_simps[n]->getNot(init_simps[n]->getMemberVertex(adj_i));
				vta_adj_temp.setParent(init_simps[n]->getDirAdj()[adj_i]);
				vta_adj.insert(vta_adj_temp);
			}
			if (to_dig) digCavity(vert_to_add, to_dig, to_dig->getUnique(*contains_vert), cavity);
			if (cavity.size() > 1) {
				fillCavity(vta_adj, cavity);
				Graph<D, N, T>::removeSimplex(init_simps[n]);
			}
		}
		Graph<D, N, T>::removeSimplex(contains_vert);

	}
	/*
	 * v is the added vertex
	 * f is the Dd facet to test against
	 * w is the vertex of f that is not on the cavity boundary (undergoes circumcenter test against Dd facet between v and
	 * remaining vertices of f)
	 * boundary is a set of (D-d) facets CONFIRMED to be part of the boundary
	 */
	void digCavity(Vec<D, T>* v, Simplex<D, N, T>* f, Vec<D, T>* w, std::set<Simplex<D, N-1, T>>& boundary) {
		Simplex<D, N-1, T> poss_bound = f->getNot(w);
		Simplex<D, N, T> to_circum_test(&poss_bound, v);
		float c = to_circum_test.circumcenterTest(*w);
		std::cout << "digcav called with circumcenter test " << c << std::endl;
		if (c > 0) {
			poss_bound.setParent(f);
			boundary.insert(poss_bound);
			std::cout << "added circum bound " << poss_bound.getMemberVertex(0)->to_string() << " -> " << poss_bound.getMemberVertex(1)->to_string() << std::endl;
		}
		else if (c < 0) {
			Simplex<D, N, T>* next;
			for (Dim n = 0; n < N; n++) {
				next = f->getDirAdj()[n];
				if (next && !next->contains(v)) {  // TODO: is this the best way to prevent back-tracking?
					digCavity(v, next, next->getUnique(*f), boundary); // this is just as fast as looking up adjacency indices for implicit member vertex index 
				}
				else if (!next) {
					Vec<D, T>* simp_temp[N-1];
					for (Dim n2 = 1; n2 < N; n2++)
						simp_temp[n2 - 1] = f->getMemberVertex((n + n2) % N);
					boundary.insert(Simplex<D, N-1, T>(&simp_temp[0]));
					std::cout << "added null bound " << simp_temp[0]->to_string() << " -> " << simp_temp[1]->to_string() << std::endl;
				}
			}
			// TODO: do we need to swap simplex adjacencies equal to f to nullptr?
			// prob not actually, fillcav looks these up by vertex orders
/*
			for (Dim adj_i = 0; adj_i < N; adj_i++) {
				if (f->getDirAdj()[adj_i]) f->getDirAdj()[adj_i]->swapDirAdj(f, nullptr);
			}
*/
			Graph<D, N, T>::removeSimplex(f);
		}
		else {
			FatalError("Circumcenter test exactly 0, unimplemented").raise();
		}
	}
	void fillCavity(std::set<Simplex<D, N-1, T>>& v_adj_f, std::set<Simplex<D, N-1, T>>& boundary) {
		std::cout << "fill cav called" << std::endl;
		if (v_adj_f.size() == 0) return;
		else {
			Simplex<D, N-1, T> f1 = *v_adj_f.begin(), f2;
			bool f2_found = false;
			for (const Simplex<D, N-1, T>& s : boundary) {
				if (f1.nOverlap(s) == N - 2) {
					f2 = s;
					f2_found = true;
					break;
				}
			}
			if (!f2_found) 
				FatalError("Couldn't find second facet for fillCav").raise();

			std::cout << "using f1 " << f1.getMemberVertex(0)->to_string() << " -> " << f1.getMemberVertex(1)->to_string() << std::endl;
			std::cout << "found f2 " << f2.getMemberVertex(0)->to_string() << " -> " << f2.getMemberVertex(1)->to_string() << std::endl;

			Vec<D, T>* vec_temp[N];
			Simplex<D, N, T>* adj_temp[N];
			vec_temp[0] = f1.getUnique(f2);
			adj_temp[0] = f2.getParent();
			vec_temp[1] = f2.getUnique(f1);
			adj_temp[1] = f1.getParent();
			Simplex<D, N-2, T> overlap = f1.getNot(vec_temp[0]);
			// Dim n_offset = 0;
			for (Dim n = 0; n < N - 2; n++) {
				// if (f1.getMemberVertex(n) == vec_temp[0]) n_offset++;
				// vec_temp[n + 2] = f1.getMemberVertex(n + n_offset);
				vec_temp[n + 2] = overlap.getMemberVertex(n);
				std::cout << "add'l vertex " << vec_temp[n + 2]->to_string() << std::endl;
				adj_temp[n + 2] = nullptr; // this side is on the cavity side 
			}
			Simplex<D, N, T>* next_simp = new Simplex<D, N, T>(&vec_temp[0]);
			for (Dim n = 0; n < N; n++) {
				next_simp->setDirAdj(n, adj_temp[n]);
				if (adj_temp[n]) {
					adj_temp[n]->swapDirAdj(next_simp);
				}
			}
			
			Graph<D, N, T>::addSimplex(next_simp);

			boundary.erase(f2);
			v_adj_f.erase(f1);

			Vec<D, T>* vec_temp2[N-1];
			vec_temp2[0] = f1.getUnique(f2);
			vec_temp2[1] = f2.getUnique(f1);
			Simplex<D, N-1, T> next_bound;
			for (Dim n = 0; n < N - 2; n++) {
				for (Dim n2 = 0; n2 < N - 3; n2++) 
					vec_temp2[n2 + 2] = overlap.getMemberVertex((n + n2) % (N - 2));
				next_bound = Simplex<D, N-1, T>(&vec_temp2[0]);
				next_bound.setParent(next_simp);
				boundary.insert(next_bound);
			}
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

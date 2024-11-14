#include "PhysicsHandler.h"

Collider& Collider::operator=(Collider rhs) {
	std::cout << "Collider::operator=" << std::endl;
	std::swap(p, rhs.p);
	std::swap(dp, rhs.dp);
	std::swap(ddp, rhs.ddp);
	std::swap(lp, rhs.lp);
	std::swap(r, rhs.r);
	std::swap(dr, rhs.dr);
	std::swap(ddr, rhs.ddr);
	std::swap(m, rhs.m);
	std::swap(type, rhs.type);
	return *this;
}

void Collider::update(float dt) {
	lp = p;
	p += dp * dt;
	dp += ddp * dt;
	r += dr * dt;
	dr += ddr * dt;
}

void Collider::applyMomentum(glm::vec3 po) {
	if (m == std::numeric_limits<float>::infinity()) return;
	if (m == 0) return; // idk what to do here, either infinite velocity or none
	dp += po / m; // should probably have a carve-out for m = 0 or inf
}

void Collider::applyForce(glm::vec3 F) {
	if (m == std::numeric_limits<float>::infinity()) return;
	if (m == 0) return; // idk what to do here, either infinite acceleration or none
	ddp += F / m; // should probably have a carve-out for m = 0 or inf
}

PointCollider::PointCollider() : Collider() {
	type = COLLIDER_TYPE_POINT;
}

MeshCollider::MeshCollider() : Collider(), vertices(nullptr), tris(nullptr), numv(0), numt(0) {
	type = COLLIDER_TYPE_MESH;
}

MeshCollider::MeshCollider(const char* f) : MeshCollider() {
	loadOBJ(f);
}

MeshCollider::~MeshCollider() {
	deleteInnards();
}

MeshCollider& MeshCollider::operator=(const MeshCollider& rhs) {
	FatalError("MeshCollider copy assignment not implemented yet lol").raise();
	Collider::operator=(rhs);
	deleteInnards();
	// would have to hook up adjacencies too...
	memcpy(vertices, rhs.vertices, rhs.numv * sizeof(Vertex));
	numv = rhs.numv;
	memcpy(tris, rhs.tris, rhs.numt * sizeof(Tri));
	return *this;
}

MeshCollider& MeshCollider::operator=(MeshCollider&& rhs) {
	std::cout << "MeshCollider::operator=" << std::endl;
	Collider::operator=(rhs);
	std::swap(vertices, rhs.vertices);
	rhs.vertices = nullptr;
	std::swap(tris, rhs.tris);
	rhs.tris = nullptr;
	std::swap(numv, rhs.numv);
	rhs.numv = 0;
	std::swap(numt, rhs.numt);
	rhs.numt = 0;
	std::cout << this << ": tris = " << tris << std::endl;
	return *this;
}

void MeshCollider::deleteInnards() {
	if (tris && numt > 0) {
		for (size_t ti = 0; ti < numt; ti++) {
			if (tris[ti].adj && tris[ti].numadj > 0) free(tris[ti].adj);
		}
		delete[] tris;
	}
	if (vertices && numv > 0) {
		for (size_t vi = 0; vi < numv; vi++) {
			if (vertices[vi].t && vertices[vi].numt > 0) delete[] vertices[vi].t;
		}
		delete[] vertices;
	}
}

// very similar to code in Mesh.cpp
// if you change anything over there, check if it should be changed here too!
void MeshCollider::loadOBJ(const char* fp) {
	std::vector<unsigned int> vertexindices, uvindices, normalindices;
	std::vector<Vertex> verticestemp;
	FILE* obj = fopen(fp, "r");
	if (!obj) FatalError("Couldn't open OBJ file").raise();
	const int expectedmatches = 3;
	char lineheader[128] = "";
	int res = fscanf(obj, "%s", lineheader), matches;
	glm::vec3 vertex;
	unsigned int vertidx[3];
	while (strcmp(lineheader, "f") != 0) {
		if (res == EOF) break;
		if (strcmp(lineheader, "v") == 0) {
			fscanf(obj, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z);
			verticestemp.push_back({vertex, nullptr, 0});
		} 
		res = fscanf(obj, "%s", lineheader);
	}
	numv = verticestemp.size();
	vertices = new Vertex[numv];
	memcpy(vertices, verticestemp.data(), numv * sizeof(Vertex));
	std::vector<Tri> tristemp;
	while (res != EOF) {
		if (strcmp(lineheader, "f") == 0) {
			matches = fscanf(obj, "%d/%*d/%*d %d/%*d/%*d %d/%*d/%*d", &vertidx[0], &vertidx[1], &vertidx[2]);
			if (matches != expectedmatches) {
				FatalError("Malformed OBJ file").raise();
				return;
			}
			// minus one cuz objs are 1-indexed :/
			tristemp.push_back({
				{vertices + vertidx[0] - 1, vertices + vertidx[1] - 1, vertices + vertidx[2] - 1},
				nullptr, 0
			});
		}
		res = fscanf(obj, "%s", lineheader);
	}
	numt = tristemp.size();
	tris = new Tri[numt];
	memcpy(tris, tristemp.data(), numt * sizeof(Tri));

	for (size_t ti = 0; ti < numt; ti++) {
		for (uint8_t vi = 0; vi < 3; vi++) {
			tris[ti].v[vi]->numt++;
			if (tris[ti].v[vi]->numt == 1) 
				tris[ti].v[vi]->t = static_cast<Tri**>(malloc(tris[ti].v[vi]->numt * sizeof(Tri*)));
			else 
				tris[ti].v[vi]->t = static_cast<Tri**>(realloc(
					tris[ti].v[vi]->t, 
					tris[ti].v[vi]->numt * sizeof(Tri*)));
			tris[ti].v[vi]->t[tris[ti].v[vi]->numt - 1] = &tris[ti];
		}
	}
	std::vector<Tri*> adjtemp;
	bool unique;
	for (size_t ti = 0; ti < numt; ti++) {
		adjtemp = std::vector<Tri*>();
		for (uint8_t vi = 0; vi < 3; vi++) {
			tris[ti].e[vi] = tris[ti].v[(vi + 1) % 3]->p - tris[ti].v[vi]->p; 
			for (size_t ati = 0; ati < tris[ti].v[vi]->numt; ati++) {
				if (&tris[ti] == tris[ti].v[vi]->t[ati]) continue;
				unique = true;
				for (Tri* oa : adjtemp) {
					if (oa == tris[ti].v[vi]->t[ati]) {
						unique = false;
						break;
					}
				}
				if (unique) adjtemp.push_back(tris[ti].v[vi]->t[ati]);
			}
		}
		tris[ti].numadj = adjtemp.size();
		tris[ti].adj = new Tri*[tris[ti].numadj];
		memcpy(tris[ti].adj, adjtemp.data(), tris[ti].numadj * sizeof(Tri*));
		tris[ti].n = glm::normalize(glm::cross(tris[ti].e[0], tris[ti].e[1]));
	}
}

ColliderPair::ColliderPair(Collider* col1, Collider* col2) : c1(col1), c2(col2), f(COLLIDER_PAIR_FLAG_NONE) {
	setCollisionFunc();
}

ColliderPair& ColliderPair::operator=(ColliderPair rhs) {
	std::swap(c1, rhs.c1);
	std::swap(c2, rhs.c2);
	std::swap(cf, rhs.cf);
	std::swap(nearest, rhs.nearest);
	return *this;
}

void ColliderPair::setCollisionFunc() {
	// prob a better way to do this, maybe with macros
	if (c1->getType() == COLLIDER_TYPE_POINT) {
		if (c2->getType() == COLLIDER_TYPE_POINT) 
			cf = std::bind(&ColliderPair::collidePointPoint, this);
		else if (c2->getType() == COLLIDER_TYPE_MESH) {
			nearest = static_cast<const Tri*>(static_cast<MeshCollider*>(c2)->getTris());
			// cf = std::bind(&ColliderPair::collidePointMesh, this);
			cf = [this] () { collidePointMesh(); };
		}
	}
	else if (c2->getType() == COLLIDER_TYPE_MESH) {
		if (c2->getType() == COLLIDER_TYPE_POINT) {
			std::swap(c1, c2);
			nearest = static_cast<const Tri*>(static_cast<MeshCollider*>(c2)->getTris());
			cf = std::bind(&ColliderPair::collidePointMesh, this);
		}
		if (c2->getType() == COLLIDER_TYPE_MESH) {
			cf = std::bind(&ColliderPair::collideMeshMesh, this);
		}
	}
}

void ColliderPair::check() const {
	cf();
}

bool ColliderPair::testPointTri(const PointCollider& p, const Tri& t) {
	// while certain optimizations /could/ be made for a strictly gravity and input driven "walking"
	// point for a camera on ground, we aim for this algorithm to be efficient (when combined with
	// a well-crafted bounding mesh) enough that they are comperable
	
	/* 
	 * a lot of analysis to be done here as to which criteria narrow the answer space more
	 * could even unfold everything
	 */

	// this technique also assumes a CCW front-face; for CW, swap the inequalities
	glm::vec3 v;
	for (uint8_t i = 0; i < 3; i++) {
		/*
		std::cout << "n = <" << t.n.x << ", " << t.n.y << ", " << t.n.z << ">" << std::endl;
		std::cout << "last: " << std::endl;
		v = p.getLastPos() - t.v[i]->p;
		std::cout << "<" << t.e[i].x << ", " << t.e[i].y << ", " << t.e[i].z << "> x <"
			<< v.x << ", " << v.y << ", " << v.z << "> = <";
		v = glm::cross(t.e[i], v);
		std::cout << v.x << ", " << v.y << ", " << v.z << ">" << std::endl; 
		*/
		v = p.getLastPos() - t.v[i]->p;
		if (glm::dot(glm::cross(t.e[i], v), t.n) < 0) return false;
		// std::cout << glm::dot(v, t.n) << std::endl;
		if (i == 0 && glm::dot(v, t.n) < 0) return false;
		/*
		std::cout << "current: " << std::endl;
		v = p.getPos() - t.v[i]->p;
		std::cout << "<" << t.e[i].x << ", " << t.e[i].y << ", " << t.e[i].z << "> x <"
			<< v.x << ", " << v.y << ", " << v.z << "> = <";
		v = glm::cross(t.e[i], v);
		std::cout << v.x << ", " << v.y << ", " << v.z << ">" << std::endl; 
		*/
		v = p.getPos() - t.v[i]->p;
		if (glm::dot(glm::cross(t.e[i], (p.getPos() - t.v[i]->p)), t.n) < 0) return false;
		// std::cout << glm::dot(v, t.n) << std::endl;
		if (i == 0 && glm::dot(v, t.n) > 0) return false;
	}
	return true;
}

bool ColliderPair::pointTriPossible(const PointCollider& p, const Tri& t) {
	// TODO: consider logic and optimization of this function
	// can we make this constexpr??
	glm::vec3 ldp = p.getPos() - p.getLastPos();
	return glm::dot(ldp, p.getPos() - t.v[0]->p) >= 0
		 || glm::dot(ldp, p.getPos() - t.v[1]->p) >= 0
		 || glm::dot(ldp, p.getPos() - t.v[2]->p) >= 0;
}

void ColliderPair::collidePointPoint() {
	FatalError("Point-point collision not yet supported").raise();
}

void ColliderPair::collidePointMesh() {
	// FatalError("Point-mesh collision not yet supported").raise();
	PointCollider* p = static_cast<PointCollider*>(c1);
	MeshCollider* m = static_cast<MeshCollider*>(c2);
	auto handlecollision = [this, p, m] (const Tri* t) {
		// f |= COLLIDER_PAIR_FLAG_CONTACT;
		float po = glm::dot(p->getMomentum(), -t->n) + glm::dot(m->getMomentum(), t->n);
		// multiplying by 2 here cancels the previous momentum and adds the deflected
		p->applyMomentum(2 * po * t->n);
		m->applyMomentum(2 * po * -t->n); // is there more efficiency to be had here???
		nearest = static_cast<const void*>(t);
	};
	if (testPointTri(*p, *static_cast<const Tri*>(nearest))) {
		std::cout << "collision with nearest!" << std::endl;
		handlecollision(static_cast<const Tri*>(nearest));
		// f |= COLLIDER_PAIR_FLAG_CONTACT;
		return;
	}
	std::set<const Tri*> searched = {static_cast<const Tri*>(nearest)}, 
		searching = {static_cast<const Tri*>(nearest)}, 
		nextsearch;
	bool looking = true;

	/*
	 * adding a feasibility dot-product check adds three dot products and some algebra to every
	 * tri, reasonable or not, but on a sufficiently big mesh this is better than no restriction.
	 *
	 * on a smaller mesh where this isnt better, we wouldn't be worried about performance issues
	 * anyway
	 */

	/*
	 * if FLAG_CONTACT is set, it's probably worth having a slightly different collsion check
	 * at the very least the flag can be used to modulate forces/accelerations during the update phase
	 */

	/*
	 * could use a "closest" instead of "last" system, allowing for more efficient computation coming
	 * from a no-collision situation...
	 */

	while (looking) {
		nextsearch = std::set<const Tri*>();
		for (const Tri* t : searching) {
			for (size_t ai = 0; ai < t->numadj; ai++) {
				if (!searched.contains(t->adj[ai]) && pointTriPossible(*p, *t->adj[ai])) 
					nextsearch.insert(t->adj[ai]);
			}
		}
		// std::cout << "searching " << nextsearch.size() << " remaining adjacencies" << std::endl;
		if (nextsearch.size() == 0) {
			std::cout << "ran out of things to search" << std::endl;
			/* 
			 * note that this non-collision condition requires checking against
			 * every triangle every frame there is no collision
			 */
			return; /* handle no collision */
		}
		searching = nextsearch;
		for (const Tri* t : searching) {
			if (testPointTri(*p, *t)) {
				// assumes single possible collision
				// multiple is very unlikely, would require a very precise
				// point or edge position
				// not even any downsides i can think of
				/* set last col */
				/* set col flags */
				/* set relevent positions */
				handlecollision(t);
				nearest = t;
				std::cout << "collision with " << t << std::endl;
				return;
			}
			else {
				searched.insert(t);
			}
		}
	}
	/* is it possible to reach this point? */
}

void ColliderPair::collideMeshMesh() {
	FatalError("Mesh-mesh collision not yet supported").raise();
}


PhysicsHandler::PhysicsHandler() : numcolliders(0), dt(0) {
	ti = (float)SDL_GetTicks() / 1000.f;
	std::cout << "init'd at " << ti << std::endl;
	lastt = ti;
}

void PhysicsHandler::start() {
	lastt = (float)SDL_GetTicks() / 1000.f;
	std::cout << "started at " << lastt << std::endl;
}

void PhysicsHandler::update() {
	// if we reworked this slightly we could multithread/parallelize it...
	dt = (float)SDL_GetTicks() / 1000.f - lastt;
	std::cout << "dt = " << dt << std::endl;
	lastt += dt;
	for (size_t i = 0; i < numcolliders; i++) {
		colliders[i].update(dt);
	}
	for (const ColliderPair& p : pairs) {
		p.check();
	}
}

/*
Collider* PhysicsHandler::addCollider(const Collider& c) {
	colliders[numcolliders] = c;
	numcolliders++;
	return &colliders[numcolliders - 1];
}
*/

void PhysicsHandler::addColliderPair(ColliderPair&& p) {
	pairs.push_back(p);
}


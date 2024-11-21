#include "PhysicsHandler.h"

Collider& Collider::operator=(Collider rhs) {
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
	dp += ddp * dt;
	p += dp * dt;
	dr += ddr * dt;
	r += dr * dt;
}

void Collider::updateCollision(float dt0, float dt1, glm::vec3 mom) {
	// see if you can shove these into dp somehow to avoid allocs
	glm::vec3 dp0 = (p - lp) / (dt0 + dt1);
	p = lp + dp0 * dt0;

	dp = mom / m + ddp * (dt0 + dt1);
	p += dp * dt1;
}

void Collider::updateContact(glm::vec3 nf, float dt0, float dt1) {
	glm::vec3 dp0 = (p - lp) / (dt0 + dt1);
	p = lp + dp0 * dt0;
	dp = dp0 + ddp * dt0;
	applyForce(nf);
	dp += ddp * dt1;
	dp -= glm::dot(dp, glm::normalize(nf)) * glm::normalize(nf);
	p += dp * dt1;
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

glm::vec3 Collider::getMomentum() const {
	if (m == std::numeric_limits<float>::infinity()) return glm::vec3(0);
	return dp * m;
}

glm::vec3 Collider::getForce() const {
	if (m == std::numeric_limits<float>::infinity()) return glm::vec3(0);
	return ddp * m;
}

PointCollider::PointCollider() : Collider() {
	type = COLLIDER_TYPE_POINT;
}

PlaneCollider::PlaneCollider(glm::vec3 norm) : Collider(), n(norm) {
	type = COLLIDER_TYPE_PLANE;
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
	Collider::operator=(rhs);
	std::swap(vertices, rhs.vertices);
	rhs.vertices = nullptr;
	std::swap(tris, rhs.tris);
	rhs.tris = nullptr;
	std::swap(numv, rhs.numv);
	rhs.numv = 0;
	std::swap(numt, rhs.numt);
	rhs.numt = 0;
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
	cf = nullptr;
	if (c1->getType() == COLLIDER_TYPE_POINT) {
		if (c2->getType() == COLLIDER_TYPE_PLANE) {
			cf = [this] (float dt) { collidePointPlane(dt); };
		}
		else if (c2->getType() == COLLIDER_TYPE_MESH) {
			nearest = static_cast<const Tri*>(static_cast<MeshCollider*>(c2)->getTris());
			cf = [this] (float dt) { collidePointMesh(dt); };
		}
	}
	else if (c1->getType() == COLLIDER_TYPE_PLANE) {
		if (c2->getType() == COLLIDER_TYPE_POINT) {
			std::swap(c1, c2);
			cf = [this] (float dt) { collidePointMesh(dt); };
		}
	}
	else if (c2->getType() == COLLIDER_TYPE_MESH) {
		if (c2->getType() == COLLIDER_TYPE_POINT) {
			std::swap(c1, c2);
			nearest = static_cast<const Tri*>(static_cast<MeshCollider*>(c2)->getTris());
			cf = [this] (float dt) { collidePointMesh(dt); };
		}
	}

	if (!cf) {
		FatalError("Unsupported ColliderPair Collider Type combination").raise();
	}
}

void ColliderPair::check(float dt) const {
	cf(dt);
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

void ColliderPair::collide(float dt, const glm::vec3& p, const glm::vec3& n) {
	glm::vec3 p0top1 = c1->getPos() - c1->getLastPos();
	float dt0 = dt * glm::length(p - c1->getLastPos()) / glm::length(p0top1);
	glm::vec3 dp0 = p0top1 / dt;
	float po1 = c1->getMass() != std::numeric_limits<float>::infinity() ?
			glm::dot((dp0 + c1->getAcc() * dt0) * c1->getMass(), -n) : 0,
		po2 = c2->getMass() != std::numeric_limits<float>::infinity() ?
			glm::dot(((c2->getPos() - c2->getLastPos()) / dt + c2->getAcc() * dt0) * c2->getMass(), n) : 0,
		po = (po1 * (float)c1->getDamp() + po2 * (float)c2->getDamp()) / 255.f;

		if (f & COLLIDER_PAIR_FLAG_CONTACT) {
		if (po > PH_CONTACT_THRESHOLD) {
			FatalError("Objects no longer in contact").raise();
		}
		else {

		}
	}
	else if (po < PH_CONTACT_THRESHOLD) {
		f |= COLLIDER_PAIR_FLAG_CONTACT;
		nf = c1->getForce() + c2->getForce();
		glm::vec3 xprime = glm::normalize(glm::vec3(n.y, -n.x, 0));
		glm::vec3 zprime = glm::cross(xprime, n);
		// could probably avoid storing xprime and doing cross for zprime
		/*
		nf = glm::vec3(
			glm::dot(nf, xprime), 
			glm::dot(nf, n), 
			glm::dot(nf, zprime)
		);
		*/
		nf = glm::vec3(0, glm::dot(nf, n), 0);
		// it seems like there are a lot of simplifications to make here
		nf = glm::vec3(
			glm::dot(nf, glm::vec3(xprime.x, 0, 0)),
			glm::dot(nf, glm::vec3(0, n.y, 0)),
			glm::dot(nf, glm::vec3(0, 0, zprime.z))
		);
		c1->updateContact(-nf, dt0, dt - dt0);
		c2->updateContact(nf, dt0, dt - dt0);
	}
	else {
		/* TODO: there are still losses in this system, figure out what they are */
		c1->updateCollision(dt0, dt - dt0, c1->getMomentum() + (po1 + po) * n);
		c2->updateCollision(dt0, dt - dt0, c2->getMomentum() + (po2 + po) * -n);
	}
}

void ColliderPair::collidePointPlane(float dt) {
	PointCollider* pt = static_cast<PointCollider*>(c1);
	PlaneCollider* pl = static_cast<PlaneCollider*>(c2);

	if (glm::dot(pt->getLastPos() - pl->getLastPos(), pl->getNorm()) > 0
		 && glm::dot(pt->getPos() - pl->getPos(), pl->getNorm()) < 0) {
		glm::vec3 colpos = pt->getLastPos()
			 - glm::dot(pl->getPos() - pt->getLastPos(), pl->getNorm()) // TODO: pl->getPos() should actually be a sub-pos at collision time
			 / glm::dot(pt->getLastPos() - pt->getPos(), pl->getNorm()) 
			 * (pt->getLastPos() - pt->getPos()); 
		collide(dt, colpos, pl->getNorm());
	}
}

void ColliderPair::collidePointMesh(float dt) {
	// TODO: make this work if the mesh's position and/or rotation has changed [l]
	/*
	 * TODO: stress-testing more complex meshes
	 */
	PointCollider* p = static_cast<PointCollider*>(c1);
	MeshCollider* m = static_cast<MeshCollider*>(c2);
	auto handlecollision = [this, p, m, dt] (const Tri* t) {
		glm::vec3 p0top1 = p->getPos() - p->getLastPos();
		float dt0 = -glm::dot(p->getLastPos() - t->v[0]->p, t->n) / glm::dot(p0top1, t->n) * dt;
		glm::vec3 dp0 = p0top1 / dt;
		float po = 0;
		if (p->getMass() != std::numeric_limits<float>::infinity())
			po += glm::dot((dp0 + p->getAcc() * dt0) * p->getMass() * (float)p->getDamp() / 255.f, -t->n);
		if (m->getMass() != std::numeric_limits<float>::infinity())
			po += glm::dot(((m->getPos() - m->getLastPos()) / dt + m->getAcc() * dt0) * m->getMass() * (float)m->getDamp() / 255.f, t->n);
		if (f & COLLIDER_PAIR_FLAG_CONTACT) {
			if (po > PH_CONTACT_THRESHOLD) {
				FatalError("Objects no longer in contact").raise();
			}
			else {

			}
		}
		else if (po < PH_CONTACT_THRESHOLD) {
			/*
			 * I still see some sketchy deceleration in the non-normal directions
			 */
			f |= COLLIDER_PAIR_FLAG_CONTACT;
			nf = p->getForce() + m->getForce();
			glm::vec3 xprime = glm::normalize(glm::vec3(t->n.y, -t->n.x, 0));
			glm::vec3 zprime = glm::cross(xprime, t->n);
			// could probably avoid storing xprime and doing cross for zprime
			/*
			nf = glm::vec3(
				glm::dot(nf, xprime), 
				glm::dot(nf, t->n), 
				glm::dot(nf, zprime)
			);
			*/
			nf = glm::vec3(0, glm::dot(nf, t->n), 0);
			// it seems like there are a lot of simplifications to make here
			nf = glm::vec3(
				glm::dot(nf, glm::vec3(xprime.x, 0, 0)),
				glm::dot(nf, glm::vec3(0, t->n.y, 0)),
				glm::dot(nf, glm::vec3(0, 0, zprime.z))
			);
			p->updateContact(-nf, dt0, dt - dt0);
			m->updateContact(nf, dt0, dt - dt0);
		}
		else {
			/* TODO: there are still losses in this system, figure out what they are */
			// can avoid some multiplication by storing po * t->n and then negating it
			p->updateCollision(dt0, dt - dt0, po * t->n);
			m->updateCollision(dt0, dt - dt0, po * -t->n);
		}
		// nearest = static_cast<const void*>(t);
	};
	if (testPointTri(*p, *static_cast<const Tri*>(nearest))) {
		handlecollision(static_cast<const Tri*>(nearest));
		return;
	}
	std::set<const Tri*> searched = {static_cast<const Tri*>(nearest)}, 
		searching = {static_cast<const Tri*>(nearest)}, 
		nextsearch;
	bool looking = true;

	while (looking) {
		nextsearch = std::set<const Tri*>();
		for (const Tri* t : searching) {
			for (size_t ai = 0; ai < t->numadj; ai++) {
				if (!searched.contains(t->adj[ai]) && pointTriPossible(*p, *t->adj[ai])) 
					nextsearch.insert(t->adj[ai]);
			}
		}
		if (nextsearch.size() == 0) {
			if (f & COLLIDER_PAIR_FLAG_CONTACT) {
				f &= ~COLLIDER_PAIR_FLAG_CONTACT;
				p->applyForce(nf);
				m->applyForce(-nf);
			}
			/* 
			 * note that this non-collision condition requires checking against
			 * every triangle every frame there is no collision
			 */
			return;
		}
		searching = nextsearch;
		for (const Tri* t : searching) {
			if (testPointTri(*p, *t)) {
				handlecollision(t);
				nearest = t;
				return;
			}
			else searched.insert(t);
		}
	}
	/* is it possible to reach this point? */
}

PhysicsHandler::PhysicsHandler() : numcolliders(0), dt(0) {
	ti = (float)SDL_GetTicks() / 1000.f;
	lastt = ti;
}

void PhysicsHandler::start() {
	lastt = (float)SDL_GetTicks() / 1000.f;
}

void PhysicsHandler::update() {
	// if we reworked this slightly we could multithread/parallelize it...
	dt = (float)SDL_GetTicks() / 1000.f - lastt;
	lastt += dt;
	for (size_t i = 0; i < numcolliders; i++) {
		colliders[i].update(dt);
	}
	for (const ColliderPair& p : pairs) {
		p.check(dt);
	}
}

void PhysicsHandler::addColliderPair(ColliderPair&& p) {
	pairs.push_back(p);
}


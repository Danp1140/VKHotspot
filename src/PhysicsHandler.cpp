#include "PhysicsHandler.h"

void swap(Collider& lhs, Collider& rhs) {
	std::swap(lhs.p, rhs.p);
	std::swap(lhs.dp, rhs.dp);
	std::swap(lhs.ddp, rhs.ddp);
	std::swap(lhs.lp, rhs.lp);
	std::swap(lhs.m, rhs.m);
	std::swap(lhs.type, rhs.type);
}

Collider& Collider::operator=(Collider rhs) {
#ifdef PH_VERBOSE_COLLIDER_OBJECTS
	std::cout << this << ": Collider::=" << std::endl;
#endif
	swap(*this, rhs);
	return *this;
}

void Collider::update(float dt) {
	lp = p;
	dp += ddp * dt;
	p += dp * dt;
}

void Collider::updateCollision(float dt0, float dt1, glm::vec3 mom) {
	// see if you can shove these into dp somehow to avoid allocs
	glm::vec3 dp0 = (p - lp) / (dt0 + dt1);
	p = lp + dp0 * dt0;

	dp = mom / m + ddp * (dt0 + dt1);
	p += dp * dt1;
}

void Collider::updateContact(glm::vec3 nf, float dt0, float dt1) {
	// TODO: efficiency; redundant normalization of nf
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

void Collider::coerceMomentum(glm::vec3 po, float dt) {
	if (m == std::numeric_limits<float>::infinity()) return;
	if (m == 0) return;
	p += po / m * dt;
	dp += po / m;
}

void Collider::applyForce(glm::vec3 F) {
	if (m == std::numeric_limits<float>::infinity()) return;
	if (m == 0) return; // idk what to do here, either infinite acceleration or none
	ddp += F / m; // should probably have a carve-out for m = 0 or inf
}

void Collider::coerceForce(glm::vec3 F, float dt) {
	if (m == std::numeric_limits<float>::infinity()) return;
	if (m == 0) return;
	p += F / m * pow(dt, 2.f);
	dp += F / m * dt;
	ddp += F / m;
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
#ifdef PH_VERBOSE_COLLIDER_OBJECTS
	std::cout << this << ": PointCollider()" << std::endl;
#endif
	type = COLLIDER_TYPE_POINT;
}

OrientedCollider::OrientedCollider() : Collider() {
#ifdef PH_VERBOSE_COLLIDER_OBJECTS
	std::cout << this << ": OrientedCollider()" << std::endl;
#endif
	r = glm::quat(1, 0, 0, 0);
	dr = glm::quat(1, 0, 0, 0);
	ddr = glm::quat(1, 0, 0, 0);
}

void swap(OrientedCollider& lhs, OrientedCollider& rhs) {
	swap(static_cast<Collider&>(lhs), static_cast<Collider&>(rhs));
	std::swap(lhs.r, rhs.r);
	std::swap(lhs.dr, rhs.dr);
	std::swap(lhs.ddr, rhs.ddr);
}

OrientedCollider& OrientedCollider::operator=(OrientedCollider rhs) {
#ifdef PH_VERBOSE_COLLIDER_OBJECTS
	std::cout << this << ": OrientedCollider::=" << std::endl;
#endif
	std::swap(*this, rhs);
	return *this;
}

void OrientedCollider::update(float dt) {
	Collider::update(dt);
	// TODO: consider an update that doesn't rely upon TWO SLERPS PER FRAME thats pretty expensive...
	// see if NLERP is good enough?
	dr = glm::mix(dr, ddr, dt);
	r = glm::mix(r, dr, dt);
	/*
	dr = dr * (ddr * dt);
	r = r * (dr * dt);
	*/
}

void OrientedCollider::setRot(glm::quat rot) {
	/*
	 * TODO: rotation update system
	 * want it so dr and ddr are adjusted with r
	 * basically, whatever orientation change needs to happen to make r = rot,
	 * apply that to dr and ddr too
	 */
	r = rot;
	// dr = dr * (rot - r);
	/*
	dr = glm::quat(0, 0, 0, 0);
	ddr = glm::quat(0, 0, 0, 0);
	*/
	dr = r;
	ddr = r;
}

PlaneCollider::PlaneCollider() :
		OrientedCollider(),
		n(0, 1, 0) {
#ifdef PH_VERBOSE_COLLIDER_OBJECTS
	std::cout << this << ": PlaneCollider()" << std::endl;
#endif
	type = COLLIDER_TYPE_PLANE;
}

PlaneCollider::PlaneCollider(glm::vec3 norm) : PlaneCollider() {
#ifdef PH_VERBOSE_COLLIDER_OBJECTS
	std::cout << this << ": PlaneCollider(vec3)" << std::endl;
#endif
	setNorm(norm);
}

PlaneCollider& PlaneCollider::operator=(PlaneCollider rhs) {
#ifdef PH_VERBOSE_COLLIDER_OBJECTS
	std::cout << this << ": PlaneCollider::=" << std::endl;
#endif
	OrientedCollider::operator=(rhs);
	std::swap(n, rhs.n);
	return *this;
}

void PlaneCollider::update(float dt) {
	OrientedCollider::update(dt);
	// how many ops does this truly save us?
	// 4 float comps instead of a few multiplications? probably worth it...
	if (getAngVel() != glm::quat(1, 0, 0, 0)) n = getRot() * glm::vec3(0, 1, 0);
}

void PlaneCollider::setNorm(glm::vec3 norm) {
	n = glm::normalize(norm);
	std::cout << this << " norm set to [" << n.x << ", " << n.y << ", " << n.z << "]" << std::endl;
	glm::vec3 v = glm::cross(glm::vec3(0, 1, 0), n);
	float theta = asin(glm::length(v)) / 2;
	setRot(glm::quat(cos(theta), sinf(theta) * v));
}

RectCollider::RectCollider() : PlaneCollider() {
#ifdef PH_VERBOSE_COLLIDER_OBJECTS
	std::cout << this << ": RectCollider()" << std::endl;
#endif
	type = COLLIDER_TYPE_RECT;
	len = glm::vec2(2, 2);
}

RectCollider::RectCollider(glm::vec3 norm, glm::vec2 l) : RectCollider() {
#ifdef PH_VERBOSE_COLLIDER_OBJECTS
	std::cout << this << ": RectCollider(vec3, vec2)" << std::endl;
#endif
	setNorm(norm);
	len = l;
}

RectCollider& RectCollider::operator=(RectCollider rhs) {
#ifdef PH_VERBOSE_COLLIDER_OBJECTS
	std::cout << this << ": RectCollider::=" << std::endl;
#endif
	PlaneCollider::operator=(rhs);
	std::swap(len, rhs.len);
	return *this;
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

ColliderPair::ColliderPair(Collider* col1, Collider* col2) : ColliderPair() {
	c1 = col1;
	c2 = col2;
	setCollisionFunc();
}

ColliderPair& ColliderPair::operator=(ColliderPair rhs) {
	std::swap(c1, rhs.c1);
	std::swap(c2, rhs.c2);
	std::swap(cf, rhs.cf);
	std::swap(nearest, rhs.nearest);
	std::swap(nf, rhs.nf);
	std::swap(reldp, rhs.reldp);
	std::swap(lreldp, rhs.lreldp);
	std::swap(dynf, rhs.dynf);
	std::swap(oncollide, rhs.oncollide);
	std::swap(oncouple, rhs.oncouple);
	std::swap(ondecouple, rhs.ondecouple);
	std::swap(onslide, rhs.onslide);
	std::swap(preventdefault, rhs.preventdefault);
	return *this;
}

void ColliderPair::setCollisionFunc() {
	cf = nullptr;
	if (c1->getType() == COLLIDER_TYPE_POINT) {
		if (c2->getType() == COLLIDER_TYPE_PLANE) {
			cf = &ColliderPair::collidePointPlane;
		}
		else if (c2->getType() == COLLIDER_TYPE_RECT) {
			cf = &ColliderPair::collidePointRect;
		}
		else if (c2->getType() == COLLIDER_TYPE_MESH) {
			nearest = static_cast<const Tri*>(static_cast<MeshCollider*>(c2)->getTris());
			cf = &ColliderPair::collidePointMesh;
		}
	}
	// TODO: test this swapping tec
	else if (c1->getType() == COLLIDER_TYPE_PLANE) {
		if (c2->getType() == COLLIDER_TYPE_POINT) {
			std::swap(c1, c2);
			cf = &ColliderPair::collidePointPlane;
		}
	}
	else if (c1->getType() == COLLIDER_TYPE_RECT) {
		if (c2->getType() == COLLIDER_TYPE_POINT) {
			std::swap(c1, c2);
			cf = &ColliderPair::collidePointRect;
		}
	}
	else if (c2->getType() == COLLIDER_TYPE_MESH) {
		if (c2->getType() == COLLIDER_TYPE_POINT) {
			std::swap(c1, c2);
			nearest = static_cast<const Tri*>(static_cast<MeshCollider*>(c2)->getTris());
			cf = &ColliderPair::collidePointMesh;
		}
	}

	if (!cf) {
		FatalError("Unsupported ColliderPair Collider Type combination").raise();
	}
}

void ColliderPair::check(float dt) {
	lreldp = reldp;
	reldp = c1->getVel() - c2->getVel();
	netf = c1->getForce() + c2->getForce();
	(this->*cf)(dt);
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
		v = p.getLastPos() - t.v[i]->p;
		if (glm::dot(glm::cross(t.e[i], v), t.n) < 0) return false;
		// std::cout << glm::dot(v, t.n) << std::endl;
		if (i == 0 && glm::dot(v, t.n) < 0) return false;
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

void ColliderPair::newtonianCollide(float dt, const glm::vec3& p, const glm::vec3& n) {
	glm::vec3 p0top1 = c1->getPos() - c1->getLastPos();
	float dt0 = dt * glm::length(p - c1->getLastPos()) / glm::length(p0top1);
	glm::vec3 dp0 = p0top1 / dt;
	float po1 = c1->getMass() != std::numeric_limits<float>::infinity() ?
			glm::dot((dp0 + c1->getAcc() * dt0) * c1->getMass(), -n) : 0,
		po2 = c2->getMass() != std::numeric_limits<float>::infinity() ?
			glm::dot(((c2->getPos() - c2->getLastPos()) / dt + c2->getAcc() * dt0) * c2->getMass(), n) : 0,
		po = (po1 * (float)c1->getDamp() + po2 * (float)c2->getDamp()) / 255.f;

	// TODO: investigate this logic lol
	if (f & COLLIDER_PAIR_FLAG_CONTACT) {
		if (po > PH_CONTACT_THRESHOLD) {
			// FatalError("Objects no longer in contact").raise();
		}
		else {

		}
	}
	else if (po < PH_CONTACT_THRESHOLD) {
		newtonianCouple(dt, dt0, n); // if we're here, preventdefault must be false
		if (oncouple) oncouple(coupledata); 
	}
	else {
#ifdef PH_VERBOSE_COLLISIONS
		std::cout << c1 << " and " << c2 << " bounced, ||po|| = " << glm::length(po) << std::endl;
#endif
		/* TODO: there are still losses in this system, figure out what they are */
		c1->updateCollision(dt0, dt - dt0, c1->getMomentum() + (po1 + po) * n);
		c2->updateCollision(dt0, dt - dt0, c2->getMomentum() + (po2 + po) * -n);
	}
}

void ColliderPair::newtonianCouple(float dt, float dt0, const glm::vec3& n) {
	f |= COLLIDER_PAIR_FLAG_CONTACT;
	nf = c1->getForce() + c2->getForce();
	glm::vec3 xprime = glm::normalize(glm::vec3(n.y, -n.x, 0));
	// TODO: check xprime == vec3(0), then do a different basis
	glm::vec3 zprime = glm::cross(xprime, n);

	nf = glm::dot(nf, n) * n;

#ifdef PH_VERBOSE_COLLISIONS
	std::cout << c1 << " and " << c2 << " coupled, ||nf|| = [" << nf.x << ", " << nf.y << ", " << nf.z << "]" << std::endl;
#endif

	c1->updateContact(-nf, dt0, dt - dt0);
	c2->updateContact(nf, dt0, dt - dt0);
}

void ColliderPair::newtonianDecouple(float dt) {
	/*
	c1->updateContact(nf, dt0, dt - dt0);
	c2->updateContact(-nf, dt0, dt - dt0);
	*/
	// TODO: this check should be specialized to the collision function
	// this function should just be the contents of the if check
	/*
	 * TODO: this system appears to have issues with small-scale decoupling
	 * should test edge-cases
	 *
	 * also seems to have issue with zero-normal-velocity decoupling (e.g. walking off a plane)
	 */
#ifdef PH_VERBOSE_COLLISIONS
	std::cout << c1 << " and " << c2 << " decoupled" << std::endl;
#endif
	c1->applyForce(nf);
	c2->applyForce(-nf);
	f &= ~COLLIDER_PAIR_FLAG_CONTACT;
}

void ColliderPair::newtonianSlide(float dt, const glm::vec3& n) {
	// so, in one frame,
	// presume pov & rect are already coupled
	// input applies force to pov
	// pov & rect are coupled, so they must share the force proportionally to their mass
	// rect has infinite mass, so it does not move and pov gets no movement in the normal dir
	// 
	// okay, seems like the problem now is premature decoupling, allowing through-wall sliding
	// TODO: lots of cleanup here lol
	
	if (reldp == glm::vec3(0)) return;

	glm::vec3 v = -glm::dot(reldp, n) * n;
	glm::vec3 normnetf = -glm::dot(netf, n) * n;
	float totalmass = c1->getMass() + c2->getMass();
	glm::vec3 f1, f2;
	if (c1->getMass() == 0 || c2->getMass() == std::numeric_limits<float>::infinity()) f1 = glm::vec3(0);
	else f1 = normnetf * c1->getMass() / totalmass;
	if (c2->getMass() == 0 || c1->getMass() == std::numeric_limits<float>::infinity()) f2 = glm::vec3(0);
	else f2 = normnetf * c2->getMass() / totalmass;

	c1->coerceForce(f1, dt);
	c2->coerceForce(f2, dt);
	
	// nf = c1->getForce() + c2->getForce();

	/*
	if (glm::dot(reldp, n) < 0) {
		c1->coerceMomentum(c1->getMass() * v, dt);
		c2->coerceMomentum(c2->getMass() * -v, dt);

		reldp = reldp + v;
	}
*/
	reldp = reldp + v;

	// TODO: early escape for both frictiondyn == 0
	if (glm::length(reldp) > PH_FRICTION_THRESHOLD) {
		// not technically a force...
		dynf = glm::length(nf) * (c1->getFrictionDyn() + c2->getFrictionDyn())
			 * -glm::normalize(reldp) * dt;
		c1->applyMomentum(dynf);
		c2->applyMomentum(-dynf);
	}
	else if (reldp != glm::vec3(0)) {
		c1->applyMomentum(c1->getMass() * -reldp);
		c2->applyMomentum(c2->getMass() * reldp);
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
		COLLIDER_PAIR_COLLIDE_CALL(dt, colpos, pl->getNorm())
	}
	else {
		// TODO: try checking contact at top, allowing slide first, then bounds check
		if (f & COLLIDER_PAIR_FLAG_CONTACT) {
			COLLIDER_PAIR_SLIDE_CALL(dt, pl->getNorm());
			if (glm::dot(c1->getMomentum(), -glm::normalize(nf))
				 + glm::dot(c2->getMomentum(), glm::normalize(nf)) > PH_CONTACT_THRESHOLD) {
				COLLIDER_PAIR_DECOUPLE_CALL(dt)
			}
		}
	}
}

void ColliderPair::collidePointRect(float dt) {
	PointCollider* pt = static_cast<PointCollider*>(c1);
	RectCollider* rc = static_cast<RectCollider*>(c2);

	if (f & COLLIDER_PAIR_FLAG_CONTACT) {
		COLLIDER_PAIR_SLIDE_CALL(dt, rc->getNorm())
		if (glm::dot(c1->getMomentum(), rc->getNorm()) + glm::dot(c2->getMomentum(), -rc->getNorm()) > PH_CONTACT_THRESHOLD) {
			COLLIDER_PAIR_DECOUPLE_CALL(dt)
		}
		else {
			glm::vec3 testpos = pt->getPos();
			testpos -= rc->getPos();
			testpos = rc->getRot() * testpos;
			if (testpos.x < -rc->getLen().x || testpos.x > rc->getLen().x
				|| testpos.z < -rc->getLen().y || testpos.z > rc->getLen().y) {
				COLLIDER_PAIR_DECOUPLE_CALL(dt)
			}
		}
	}

	// TODO: find a way to remove redundant collision and colpos code
	if (glm::dot(pt->getLastPos() - rc->getLastPos(), rc->getNorm()) > 0
		 && glm::dot(pt->getPos() - rc->getPos(), rc->getNorm()) < 0) {
		glm::vec3 colpos = pt->getLastPos()
			 - glm::dot(rc->getPos() - pt->getLastPos(), rc->getNorm()) // TODO: rc->getPos() should actually be a sub-pos at collision time
			 / glm::dot(pt->getLastPos() - pt->getPos(), rc->getNorm()) 
			 * (pt->getLastPos() - pt->getPos());
		// TODO: same as above, rc->getPos() should be a collision time subpos
		glm::vec3 testpos = colpos;
		testpos -= rc->getPos();
		testpos = rc->getRot() * testpos;
		if (testpos.x > -rc->getLen().x && testpos.x < rc->getLen().x
			&& testpos.z > -rc->getLen().y && testpos.z < rc->getLen().y) {
			COLLIDER_PAIR_COLLIDE_CALL(dt, colpos, rc->getNorm())
			return;
		}
		if (f & COLLIDER_PAIR_FLAG_CONTACT) {
			COLLIDER_PAIR_DECOUPLE_CALL(dt)
			return;
		}
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

PhysicsHandler::PhysicsHandler() : dt(0) {
	ti = (float)SDL_GetTicks() / 1000.f;
	lastt = ti;
}

PhysicsHandler::~PhysicsHandler() {
	for (Collider* c : colliders) delete c;
}

void PhysicsHandler::start() {
	lastt = (float)SDL_GetTicks() / 1000.f;
}

void PhysicsHandler::update() {
	// if we reworked this slightly we could multithread/parallelize it...
	dt = (float)SDL_GetTicks() / 1000.f - lastt;
	lastt += dt;
	/*
	 * TODO: find a way to do this with one loop instead of two??
	 */
	for (size_t i = 0; i < tms.size(); i++) {
		if (tms[i].dt < 0) {
			tms[i].c->applyMomentum(-tms[i].v);
			tms.erase(tms.begin() + i);
		}
	}
	for (size_t i = 0; i < tms.size(); i++) tms[i].dt -= dt;
	for (size_t i = 0; i < tfs.size(); i++) {
		if (tfs[i].dt < 0) {
			tfs[i].c->applyForce(-tfs[i].v);
			tfs.erase(tfs.begin() + i);
		}
	}
	for (size_t i = 0; i < tfs.size(); i++) tfs[i].dt -= dt;
	for (Collider* c : colliders) c->update(dt);
	for (ColliderPair& p : pairs) p.check(dt);
}

void PhysicsHandler::addColliderPair(ColliderPair&& p) {
	pairs.push_back(p);
}

void PhysicsHandler::addTimedMomentum(TimedValue&& t) {
	tms.push_back(t);
	tms.back().c->applyMomentum(tms.back().v);
}

void PhysicsHandler::addTimedForce(TimedValue&& t) {
	tfs.push_back(t);
	tfs.back().c->applyForce(tfs.back().v);
}


#include <vector>
#include <set>
#include <functional>

#include <ext.hpp>
#include <SDL3/SDL.h>

#include "Errors.h"

#define PH_MAX_NUM_COLLIDERS 64
#define PH_CONTACT_THRESHOLD 0.5 // if the momentum exchanged during a collision is less than this, the objects are presumed to be in contact
/*
 * Trying smth different here. Velocity is probably more important than momentum for display,
 * although for rounding momentum could be important, esp for low-mass colliders.
 * In the future this macro can just be set equal to another macro
 */
#define PH_DECOUPLE_VELOCITY_THRESHOLD 0.5 // m/s
#define PH_FRICTION_THRESHOLD 0.5

#define PH_VERBOSE_COLLISIONS
// #define PH_VERBOSE_COLLIDER_OBJECTS

typedef enum ColliderType {
	COLLIDER_TYPE_UNKNOWN,
	COLLIDER_TYPE_POINT,
	COLLIDER_TYPE_SPHERE,
	COLLIDER_TYPE_PLANE,
	COLLIDER_TYPE_RECT,
	COLLIDER_TYPE_MESH
} ColliderType;

class Collider {
public:
	Collider() :
		p(glm::vec3(0)),
		dp(glm::vec3(0)),
		ddp(glm::vec3(0)),
		lp(glm::vec3(0)),
		m(1),
		frictiondynamic(1),
		dampening(0x55),
		type(COLLIDER_TYPE_UNKNOWN) {}
	Collider(const Collider& lvalue) = default;
	Collider(Collider&& rvalue) :
		p(std::move(rvalue.p)),
		dp(std::move(rvalue.dp)),
		ddp(std::move(rvalue.ddp)),
		lp(std::move(rvalue.lp)),
		m(std::move(rvalue.m)),
		type(std::move(rvalue.type)) {}
	~Collider() = default;

	friend void swap(Collider& lhs, Collider& rhs);

	virtual Collider& operator=(Collider rhs);

	virtual void update(float dt);
	// two-step update for both legs before and after collision
	// TODO: more in-depth descriptions of below
	virtual void updateCollision(float dt0, float dt1, glm::vec3 mom);
	virtual void updateContact(glm::vec3 nf, float dt0, float dt1);
	virtual void updateSlide(glm::vec3 nm, float dt);
	void setPos(glm::vec3 pos) {p = pos;}
	void setMass(float ma) {m = ma;}
	void setFrictionDyn(float f) {frictiondynamic = f;}
	void setDamp(uint8_t d) {dampening = d;}
	void applyMomentum(glm::vec3 po);
	void applyForce(glm::vec3 F);

	glm::vec3 getPos() const {return p;}
	glm::vec3 getVel() const {return dp;}
	glm::vec3 getAcc() const {return ddp;}
	glm::vec3 getLastPos() const {return lp;}
	float getMass() const {return m;}
	float getFrictionDyn() const {return frictiondynamic;}
	uint8_t getDamp() const {return dampening;}
	glm::vec3 getMomentum() const; 
	glm::vec3 getForce() const;
	ColliderType getType() const {return type;}

protected:
	ColliderType type;

private:
	glm::vec3 p, dp, ddp, lp; // since p, dp, and ddp are all updated together, lp must be stored and cannot be derived
	/*
	 * TODO: frictiondynamic description
	 */
	float m, frictiondynamic;
	/*
	 * dampening applied to the objects momentum contribution during collision, 
	 * 0 => all momentum diffused, 1 => all momentum transferred
	 */
	uint8_t dampening;
};

class PointCollider : public Collider {
public:
	PointCollider(); 
	PointCollider(const PointCollider& lvalue) = default;
	PointCollider(PointCollider&& rvalue) : Collider(rvalue) {}
	~PointCollider() = default;
};

class SphereCollider : public Collider {
public:
	SphereCollider();
	SphereCollider(float rad);
	SphereCollider(const SphereCollider& lvalue) = default;
	SphereCollider(SphereCollider&& rvalue) : 
		Collider(std::move(rvalue)), 
		r(std::move(rvalue.r)) {}
	~SphereCollider() = default;

	float getR() const {return r;}

private:
	float r;
};

class OrientedCollider : public Collider {
public:
	OrientedCollider();
	OrientedCollider(const OrientedCollider& lvalue) = default;
	OrientedCollider(OrientedCollider&& rvalue) : 
		Collider(rvalue),
		r(std::move(rvalue.r)),
		dr(std::move(rvalue.dr)),
		ddr(std::move(rvalue.ddr)) {}
	~OrientedCollider() = default;

	friend void swap(OrientedCollider& lhs, OrientedCollider& rhs);

	OrientedCollider& operator=(OrientedCollider rhs);

	virtual void update(float dt);

	const glm::quat& getRot() const {return r;}
	const glm::quat& getAngVel() const {return dr;}

	void setRot(glm::quat rot);

private:
	glm::quat r, dr, ddr;
};

class PlaneCollider : public OrientedCollider {
public:
	PlaneCollider();
	PlaneCollider(const PlaneCollider& lvalue) = default;
	PlaneCollider(PlaneCollider&& rvalue) :
		OrientedCollider(rvalue),
		n(std::move(rvalue.n)) {}
	PlaneCollider(glm::vec3 norm);
	~PlaneCollider() = default;

	PlaneCollider& operator=(PlaneCollider rhs);

	void update(float dt);

	// adjusts rotation as appropriate
	void setNorm(glm::vec3 norm);

	const glm::vec3& getNorm() const {return n;}

private:
	// redundant with rotation and implicit default normal of +y
	// calculated during update if dr != 0, just saves us redundant calc
	glm::vec3 n; 
};

class RectCollider : public PlaneCollider {
public:
	RectCollider();
	RectCollider(const RectCollider& lvalue) = default;
	RectCollider(RectCollider&& rvalue) :
		PlaneCollider(rvalue),
		len(std::move(rvalue.len)) {}
	RectCollider(glm::vec3 norm, glm::vec2 l);
	~RectCollider() = default;

	RectCollider& operator=(RectCollider rhs);

	const glm::vec2& getLen() const {return len;}

private:
	glm::vec2 len; // expanded ±len from center at p
};

struct Tri;

typedef struct Vertex {
	glm::vec3 p;
	Tri** t = nullptr;
	size_t numt = 0;
} Vertex;

typedef struct Tri {
	Vertex* v[3];

	Tri** adj = nullptr;
	size_t numadj = 0;
	glm::vec3 e[3], n;
} Tri;

// TODO: should be OrientedCollider
class MeshCollider : public Collider {
public:
	MeshCollider();
	MeshCollider(const char* f);
	~MeshCollider();

	MeshCollider& operator=(const MeshCollider& rhs);
	MeshCollider& operator=(MeshCollider&& rhs);

	const Tri* getTris() {return tris;}

private:
	Vertex* vertices;
	Tri* tris; 
	size_t numv, numt;

	void deleteInnards();
	void loadOBJ(const char* fp);
};

typedef enum ColliderPairFlagBits {
	COLLIDER_PAIR_FLAG_NONE = 0,
	COLLIDER_PAIR_FLAG_CONTACT = 0x01,
	COLLIDER_PAIR_FLAG_CLIPPING = 0x02
} ColliderPairFlagBits;
typedef uint8_t ColliderPairFlags;

typedef void (*PhysicsCallbackFunc)(void *);

typedef struct PhysicsCallback {
	PhysicsCallbackFunc f = nullptr;
	void* d = nullptr;
} PhysicsCallback;

#define COLLIDER_PAIR_COLLIDE_CALL(dt, cp, n) { \
	if (!preventdefault) newtonianCollide(dt, cp, n); \
	if (oncollide.f) oncollide.f(oncollide.d); \
}
#define COLLIDER_PAIR_SLIDE_CALL(dt, n) { \
	if (!preventdefault) newtonianSlide(dt, n); \
	if (onslide.f) onslide.f(onslide.d); \
}
#define COLLIDER_PAIR_DECOUPLE_CALL(dt) { \
	if (!preventdefault) newtonianDecouple(dt); \
	if (ondecouple.f) ondecouple.f(ondecouple.d); \
}

class ColliderPair {
public:
	ColliderPair() : 
		c1(nullptr), 
		c2(nullptr), 
		f(COLLIDER_PAIR_FLAG_NONE), 
		cf(nullptr),
		nearest(nullptr),
		nf(glm::vec3(0)),
		reldp(0),
		lreldp(0),
		dynf(0),
		preventdefault(false) {}
	ColliderPair(Collider* col1, Collider* col2);
	~ColliderPair() = default;

	ColliderPair& operator=(ColliderPair rhs);

	void check(float dt);

	void setOnCollide(PhysicsCallback pc) {oncollide = pc;}
	void setOnCouple(PhysicsCallback pc) {oncouple = pc;}
	void setOnDecouple(PhysicsCallback pc) {ondecouple = pc;}
	void setOnSlide(PhysicsCallback pc) {onslide = pc;}
	void setOnAntiCollide(PhysicsCallback pc) {onanticollide = pc;}
	void setOnUnclip(PhysicsCallback pc) {onunclip = pc;}
	void setOnAntiUnclip(PhysicsCallback pc) {onantiunclip = pc;}
	void setPreventDefault(bool p) {preventdefault = p;}

private:
	Collider* c1, * c2;
	ColliderPairFlags f;
	void (ColliderPair::*cf)(float);
	bool preventdefault;
	PhysicsCallback oncollide, oncouple, ondecouple, onslide, onunclip, onantiunclip, onanticollide;
	
	const void* nearest;
	glm::vec3 nf, reldp, lreldp, dynf, netf; // nf is normal force, netf is net force 
	// could eliminate contact flag by checking if nf is nonzero?

	/*
	 * Note: this function may swap c1 and c2 to make their order predictable for collision functions
	 */
	void setCollisionFunc();
	
	// could make these non-static
	static bool testPointTri(const PointCollider& p, const Tri& t);
	static bool pointTriPossible(const PointCollider& p, const Tri& t);

	void newtonianCollide(float dt, const glm::vec3& p, const glm::vec3& n);
	void newtonianCouple(float dt, float dt0, const glm::vec3& n);
	void newtonianDecouple(float dt);
	void newtonianSlide(float dt, const glm::vec3& n);

	/* Desired collision function logical guarentees:
	 *  - No positions can become NaN
	 *  - Colliders may never pass through one another
	 *   - Seems vague, but this guarentee is based on underlying geometry
	 *  - Similarly, coupled objects may not pass through one another; the only way to
	 *    move is tangent to or antiparallel to the normal
	 *     - This is mostly based on the slide call
	 *
	 */

	void collidePointPlane(float dt);
	void collidePointRect(float dt);
	void collidePointMesh(float dt);

	void collideSphereSphere(float dt);
	void collideSpherePlane(float dt);
	void collideSphereRect(float dt);
};

typedef struct TimedValue {
	Collider* c;
	glm::vec3 v;
	float dt;
} TimedMomentum;

/*
 * Prominent bugs:
 *  - If multiple colliders are intersected, they are handled in-order. Not inheretly bad, but a high-velocity
 *    object can produce predicatble but perhaps undesirable results.
 *     - Put things like kill planes last in the order for sure
 *     - Multiple bounces in a frame will never be handled correctly, as only one bounce can be handled per pair
 *        - perhaps fixable
 *  - Certain force combinations while colliding a wall can cause decoupling, allowing clip-through
 *     - Hard to predict what inputs will couse this
 *     - Likely an issue in the decoupling criteria, although if newtonianSlide isn't correctly canceling
 *       force & momentum it could allow valid decoupling criteria to be erroneously satisfied
 *  - Ramps *really* don't work anymore, after adding the sliding normal-direction cancellation
 *     - Likely fixable with some modifications to newtonianSlide
 *     - Should be fine if we *just* cancel force/momentum in the anti-normal dir
 */

class PhysicsHandler {
public:
	PhysicsHandler();
	~PhysicsHandler();

	// so that dt doesn't accumulate during init optimizations
	// call this as shortly before your first draw loop as possible
	void start();
	/*
	 * Whole frame update calculates the position of all colliders after time elapsed from previous frame.
	 */
	void update();

	/*
	 * This whole function is syntactic grossness, but basically we want to make sure the correct
	 * assigment operator override gets called so we cast things around.
	 *
	 * For large things like MeshCollider, we'd rather not realloc so we work with an rvalue here.
	 *
	 * May be prudent to put in some requirements for what type can be passed in.
	 */
	template<class T>
	Collider* addCollider(T&& c) {
		colliders.push_back(new T(c));
		return colliders.back();
	}
	ColliderPair* addColliderPair(ColliderPair&& p, bool active);
	void removeColliderPair(ColliderPair* p);
	void activateColliderPair(ColliderPair* p);
	void deactivateColliderPair(ColliderPair* p);

	// rounds down; if dt == 0, will just apply during one update cycle
	void addTimedMomentum(TimedValue&& t); 
	void addTimedForce(TimedValue&& t); 

	float getDT() {return dt;}

private:
	std::vector<Collider*> colliders;
	std::set<ColliderPair*> pairs, activepairs;
	std::vector<TimedValue> tms, tfs;

	float ti, lastt, dt; // in s
};

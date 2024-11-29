#include <vector>
#include <set>

#include <ext.hpp>
#include <SDL3/SDL.h>

#include "Errors.h"

#define PH_MAX_NUM_COLLIDERS 64
#define PH_CONTACT_THRESHOLD 0.3 // if the momentum exchanged during a collision is less than this, the objects are presumed to be in contact

#define PH_VERBOSE_COLLISIONS

typedef enum ColliderType {
	COLLIDER_TYPE_UNKNOWN,
	COLLIDER_TYPE_POINT,
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
		dampening(0x55),
		type(COLLIDER_TYPE_UNKNOWN) {}
	~Collider() = default;

	virtual Collider& operator=(Collider rhs);

	virtual void update(float dt);
	// two-step update for both legs before and after collision
	virtual void updateCollision(float dt0, float dt1, glm::vec3 mom);
	virtual void updateContact(glm::vec3 nf, float dt0, float dt1);
	void setPos(glm::vec3 pos) {p = pos;}
	void setMass(float ma) {m = ma;}
	void applyMomentum(glm::vec3 po);
	void applyForce(glm::vec3 F);

	glm::vec3 getPos() const {return p;}
	glm::vec3 getVel() const {return dp;}
	glm::vec3 getAcc() const {return ddp;}
	glm::vec3 getLastPos() const {return lp;}
	float getMass() const {return m;}
	uint8_t getDamp() const {return dampening;}
	glm::vec3 getMomentum() const; 
	glm::vec3 getForce() const;
	ColliderType getType() const {return type;}

protected:
	ColliderType type;

private:
	glm::vec3 p, dp, ddp, lp; // since p, dp, and ddp are all updated together, lp must be stored and cannot be derived
	float m;
	uint8_t dampening; // applied to the objects momentum contribution during collision, 0 => all momentum diffused, 1 => all momentum transferred
};

class PointCollider : public Collider {
public:
	PointCollider(); 
	~PointCollider() = default;

private:
};

class OrientedCollider : public Collider {
public:
	OrientedCollider() :
		Collider(),
		r(0, 0, 0, 1),
		dr(0, 0, 0, 1),
		ddr(0, 0, 0, 1) {}
	~OrientedCollider() = default;

	virtual void update(float dt);

	const glm::quat& getRot() const {return r;}
	const glm::quat& getAngVel() const {return dr;}

private:
	glm::quat r, dr, ddr;
};

class PlaneCollider : public OrientedCollider {
public:
	PlaneCollider();
	PlaneCollider(glm::vec3 norm);
	~PlaneCollider() = default;

	void update(float dt);

	const glm::vec3& getNorm() const {return n;}

private:
	glm::vec3 n; 
	// redundant with rotation and implicit default normal of +y
	// calculated during update if dr != 0, just saves us redundant calc
};

class RectCollider : public PlaneCollider {
public:
	RectCollider();
	~RectCollider() = default;

private:
	glm::vec2 len; // expanded equidistant from center at p
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
	COLLIDER_PAIR_FLAG_CONTACT = 0x01
} ColliderPairFlagBits;
typedef uint8_t ColliderPairFlags;

typedef std::function<void(float)> CollisionFunc;

class ColliderPair {
public:
	ColliderPair() : 
		c1(nullptr), 
		c2(nullptr), 
		f(COLLIDER_PAIR_FLAG_NONE), 
		cf(nullptr),
		nearest(nullptr),
		nf(glm::vec3(0)) {}
	ColliderPair(Collider* col1, Collider* col2);
	~ColliderPair() = default;

	ColliderPair& operator=(ColliderPair rhs);

	void check(float dt) const;
private:
	Collider* c1, * c2;
	ColliderPairFlags f;
	CollisionFunc cf;
	const void* nearest;
	glm::vec3 nf; // could eliminate contact flag by checking if this is nonzero?

	/*
	 * Note: this function may swap c1 and c2 to make their order predictable for collision functions
	 */
	void setCollisionFunc();
	
	// could make these non-static
	static bool testPointTri(const PointCollider& p, const Tri& t);
	static bool pointTriPossible(const PointCollider& p, const Tri& t);

	void collide(float dt, const glm::vec3& p, const glm::vec3& n);
	void contact(float dt);
	void uncontact();
	void collidePointPlane(float dt);
	void collidePointRect(float dt);
	void collidePointMesh(float dt);
};

typedef struct TimedValue {
	Collider* c;
	glm::vec3 v;
	float dt;
} TimedMomentum;

class PhysicsHandler {
public:
	PhysicsHandler();
	~PhysicsHandler() = default;

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
		*static_cast<T*>(&colliders[numcolliders]) = std::move(c);
		numcolliders++;
		return &colliders[numcolliders - 1];
	}
	void addColliderPair(ColliderPair&& p);

	// rounds down; if dt == 0, will just apply during one update cycle
	void addTimedMomentum(TimedValue&& t); 
	void addTimedForce(TimedValue&& t); 


private:
	Collider colliders[PH_MAX_NUM_COLLIDERS];
	size_t numcolliders;
	std::vector<ColliderPair> pairs;
	std::vector<TimedValue> tms, tfs;

	float ti, lastt, dt; // in s
};

#include "Scene.h"
#include "PhysicsHandler.h"
#include "InputHandler.h"

#define MOVEMENT_SENS 50.f
#define MOVEMENT_CAP 5.f
#define CAMERA_SENS 0.01f

RenderPassInfo createRenderPass(const WindowInfo& w);

PipelineInfo createViewportPipeline(const VkExtent2D& e, const VkRenderPass& r);

int main() {
	// TODO: more GH customization
	//  - request additional device exts
	//  - request specific descriptor pool characteristics
	GH gh;
	WindowInfo fpw(glm::vec2(0, 0), glm::vec2(0.5, 1)), 
		tpw(glm::vec2(0.5, 0), glm::vec2(0.5, 1));
	InputHandler ih;

	Mesh floor("../resources/models/plane.obj");
	Mesh ramp("../resources/models/plane.obj");
	Mesh wall0("../resources/models/plane.obj");
	Mesh wall1("../resources/models/plane.obj");
	Mesh event("../resources/models/plane.obj");
	Mesh cube0("../resources/models/cube.obj");
	Mesh cube1("../resources/models/cube.obj");
	Mesh povsphere("../resources/models/icosphere.obj");
	Mesh sphere("../resources/models/icosphere.obj");

	/*
	 * Setting Up Scene & Graphics Stuff
	 */
	RenderPassInfo fprp = createRenderPass(fpw),
		       tprp = createRenderPass(tpw);
	PipelineInfo fpp = createViewportPipeline(fpw.getSCExtent(), fprp.getRenderPass()),
		     tpp = createViewportPipeline(tpw.getSCExtent(), tprp.getRenderPass());
	float ar = (float)fpw.getSCExtent().width / (float)fpw.getSCExtent().height;

	Scene fps(ar), tps(ar);
	fprp.addPipeline(fpp, &fps.getCamera()->getVP());
	fps.addRenderPass(fprp);
	fps.getRenderPass(0).addMesh(&floor, VK_NULL_HANDLE, &floor.getModelMatrix(), 0);
	fps.getRenderPass(0).addMesh(&ramp, VK_NULL_HANDLE, &ramp.getModelMatrix(), 0);
	fps.getRenderPass(0).addMesh(&wall0, VK_NULL_HANDLE, &wall0.getModelMatrix(), 0);
	fps.getRenderPass(0).addMesh(&wall1, VK_NULL_HANDLE, &wall1.getModelMatrix(), 0);
	fps.getRenderPass(0).addMesh(&sphere, VK_NULL_HANDLE, &sphere.getModelMatrix(), 0);
	fps.getRenderPass(0).addMesh(&cube0, VK_NULL_HANDLE, &cube0.getModelMatrix(), 0);
	fps.getRenderPass(0).addMesh(&cube1, VK_NULL_HANDLE, &cube1.getModelMatrix(), 0);

	tprp.addPipeline(tpp, &tps.getCamera()->getVP());
	tps.addRenderPass(tprp);
	tps.getRenderPass(0).addMesh(&floor, VK_NULL_HANDLE, &floor.getModelMatrix(), 0);
	tps.getRenderPass(0).addMesh(&ramp, VK_NULL_HANDLE, &ramp.getModelMatrix(), 0);
	tps.getRenderPass(0).addMesh(&povsphere, VK_NULL_HANDLE, &povsphere.getModelMatrix(), 0);
	tps.getRenderPass(0).addMesh(&wall0, VK_NULL_HANDLE, &wall0.getModelMatrix(), 0);
	tps.getRenderPass(0).addMesh(&wall1, VK_NULL_HANDLE, &wall1.getModelMatrix(), 0);
	// tps.getRenderPass(0).addMesh(&event, VK_NULL_HANDLE, &event.getModelMatrix(), 0);
	tps.getRenderPass(0).addMesh(&sphere, VK_NULL_HANDLE, &sphere.getModelMatrix(), 0);
	tps.getRenderPass(0).addMesh(&cube0, VK_NULL_HANDLE, &cube0.getModelMatrix(), 0);
	tps.getRenderPass(0).addMesh(&cube1, VK_NULL_HANDLE, &cube1.getModelMatrix(), 0);

	tps.getCamera()->setFOVY(glm::half_pi<float>());

	fpw.addTasks(fps.getDrawTasks());
	tpw.addTasks(tps.getDrawTasks());

	/*
	 * Setting Up Physics Objects
	 */
	
	/* TODO: error when we swap order of adding pov and deathplane */
	PhysicsHandler ph;
	SphereCollider* pov = static_cast<SphereCollider*>(ph.addCollider(SphereCollider(1.f)));
	pov->setPos(glm::vec3(0, 5, 0));
	pov->applyForce(glm::vec3(0, -9.807 * pov->getMass(), 0));

	PlaneCollider* deathplane = static_cast<PlaneCollider*>(ph.addCollider(PlaneCollider(glm::vec3(0, 1, 0))));
	deathplane->setMass(std::numeric_limits<float>::infinity());
	deathplane->setPos(glm::vec3(0, -1, 0));

	RectCollider* mainstage = static_cast<RectCollider*>(ph.addCollider(RectCollider(glm::vec3(0, 1, 0), glm::vec2(10))));
	mainstage->setMass(std::numeric_limits<float>::infinity());

	bool onramp = false;
	RectCollider* rampcol = static_cast<RectCollider*>(ph.addCollider(RectCollider(glm::vec3(1, 1, 0), glm::vec2(5))));
	rampcol->setMass(std::numeric_limits<float>::infinity());
	// TODO: figure out why this is inconsistent w/ visual position of collider
	rampcol->setPos(glm::vec3(-5, 2, 5));
	ramp.setScale(glm::vec3(0.5));
	ramp.setPos(rampcol->getPos());
	ramp.setRot(rampcol->getRot());

	RectCollider* wall0col = static_cast<RectCollider*>(ph.addCollider(RectCollider(glm::vec3(1, 0, 0), glm::vec3(5))));
	wall0col->setMass(std::numeric_limits<float>::infinity());
	wall0col->setPos(glm::vec3(-9.9, 5, -5));
	wall0.setScale(glm::vec3(0.5));
	wall0.setPos(wall0col->getPos());
	wall0.setRot(wall0col->getRot());

	RectCollider* wall1col = static_cast<RectCollider*>(ph.addCollider(RectCollider(glm::vec3(0, 0, 1), glm::vec3(5))));
	wall1col->setMass(std::numeric_limits<float>::infinity());
	wall1col->setPos(glm::vec3(-5, 5, -9.9));
	wall1.setScale(glm::vec3(0.5));
	wall1.setPos(wall1col->getPos());
	wall1.setRot(wall1col->getRot());

	RectCollider* eventcol = static_cast<RectCollider*>(ph.addCollider(RectCollider(glm::vec3(0, 0, 1), glm::vec3(5))));
	eventcol->setMass(std::numeric_limits<float>::infinity());
	eventcol->setPos(glm::vec3(5, 5, -5));
	cube0.setPos(eventcol->getPos() * glm::vec3(1, 0, 1) + glm::vec3(0, 1, 2));
	cube1.setPos(eventcol->getPos() * glm::vec3(1, 0, 1) - glm::vec3(0, 0, 2));
	event.setScale(glm::vec3(0.5));
	event.setPos(eventcol->getPos());
	event.setRot(eventcol->getRot());

	SphereCollider* spherecol = static_cast<SphereCollider*>(ph.addCollider(SphereCollider(1.f)));
	spherecol->setMass(1.f);
	spherecol->setPos(glm::vec3(0, 10, -5));
	spherecol->applyForce(spherecol->getMass() * glm::vec3(0, -9.807, 0));


	ColliderPair* death = ph.addColliderPair(ColliderPair(pov, deathplane), true);
	ph.addColliderPair(ColliderPair(pov, mainstage), true);
	ph.addColliderPair(ColliderPair(pov, rampcol), true);
	ph.addColliderPair(ColliderPair(pov, wall0col), true);
	ph.addColliderPair(ColliderPair(pov, wall1col), true);
	ColliderPair* doevent = ph.addColliderPair(ColliderPair(pov, eventcol), true);
	// ph.addColliderPair(ColliderPair(pov, spherecol));

	ph.addColliderPair(ColliderPair(spherecol, deathplane), true);
	ph.addColliderPair(ColliderPair(spherecol, mainstage), true);

	death->setOnCollide({[] (void* d) {
			Collider* c = static_cast<Collider*>(d);
			c->setPos(glm::vec3(0, 5, 0));
			std::cout << "died" << std::endl;
		}, pov});
	death->setPreventDefault(true);
	Mesh* cubes[2] = {&cube0, &cube1};
	doevent->setOnCollide({[] (void* d) {
			Mesh** cubes = static_cast<Mesh**>(d);
			cubes[1]->setPos(cubes[1]->getPos() * glm::vec3(1, 0, 1) + glm::vec3(0, 1, 0));
		}, &cubes[0]});
	doevent->setOnAntiCollide({[] (void* d) {
			Mesh** cubes = static_cast<Mesh**>(d);
			cubes[0]->setPos(cubes[0]->getPos() * glm::vec3(1, 0, 1) + glm::vec3(0, 1, 0));
		}, &cubes[0]});
	doevent->setOnUnclip({[] (void* d) {
			Mesh** cubes = static_cast<Mesh**>(d);
			cubes[1]->setPos(cubes[1]->getPos() * glm::vec3(1, 0, 1) + glm::vec3(0, 0, 0));
		}, &cubes[0]});
	doevent->setOnAntiUnclip({[] (void* d) {
			Mesh** cubes = static_cast<Mesh**>(d);
			cubes[0]->setPos(cubes[0]->getPos() * glm::vec3(1, 0, 1) + glm::vec3(0, 0, 0));
		}, &cubes[0]});


	doevent->setPreventDefault(true);
	// TODO: modify pipeline to have visible but distinct antinormal

	/*
	ph.getColliderPair(2).setOnCouple([] (void* d) {

			}, &onramp);
			*/

	// TODO: prevent jump and move in air lol
	glm::vec3 movementdir;
	ih.addHold(InputHold(SDL_SCANCODE_W, [&movementdir, pov, c = fps.getCamera()] () { movementdir += glm::normalize(c->getForward() * glm::vec3(1, 0, 1)); }));
	ih.addHold(InputHold(SDL_SCANCODE_A, [&movementdir, pov, c = fps.getCamera()] () { movementdir -= c->getRight(); }));
	ih.addHold(InputHold(SDL_SCANCODE_S, [&movementdir, pov, c = fps.getCamera()] () { movementdir -= glm::normalize(c->getForward() * glm::vec3(1, 0, 1)); }));
	ih.addHold(InputHold(SDL_SCANCODE_D, [&movementdir, pov, c = fps.getCamera()] () { movementdir += c->getRight(); }));

	ih.addCheck(InputCheck(SDL_EVENT_KEY_DOWN, [&ph, pov] (const SDL_Event& e) {
		if (e.key.scancode == SDL_SCANCODE_SPACE && !e.key.repeat) {
			ph.addTimedForce({pov, glm::vec3(0, 25, 0), 0.2});
			return true;
		}
		return false;
	}));

	ih.addCheck(InputCheck(SDL_EVENT_MOUSE_MOTION, [pov, c = fps.getCamera()] (const SDL_Event& e) {
		c->setForward(c->getForward() + CAMERA_SENS * (c->getRight() * e.motion.xrel + c->getUp() * -e.motion.yrel));
		return true;
	}));

	glm::vec3 povpostemp;
	ph.start();
	while (fpw.frameCallback() && tpw.frameCallback()) {
		movementdir = glm::vec3(0);
		ih.update();
		SDL_PumpEvents();
		if (movementdir != glm::vec3(0) && glm::length(pov->getVel()) < MOVEMENT_CAP) {
			movementdir = glm::normalize(movementdir);
			ph.addTimedForce({pov, MOVEMENT_SENS * movementdir, 0});
		}

		ph.update();
		fps.getCamera()->setPos(pov->getPos());
		povsphere.setPos(pov->getPos());
		sphere.setPos(spherecol->getPos());
		event.setRot(eventcol->getRot());
		/*
		ramp.setPos(rampcol->getPos());
		ramp.setRot(rampcol->getRot());
		*/
		// std::cout << rampcol->getRot().w << ", " << rampcol->getRot().x << ", " << rampcol->getRot().y << ", " << rampcol->getRot().z << ", " << std::endl;
		// std::cout << pov->getPos().x << ", " << pov->getPos().y << ", " << pov->getPos().z << std::endl;
		// std::cout << pov->getVel().x << ", " << pov->getVel().y << ", " << pov->getVel().z << std::endl;
		// std::cout << pov->getAcc().x << ", " << pov->getAcc().y << ", " << pov->getAcc().z << std::endl;
	}

	vkQueueWaitIdle(GH::getGenericQueue());

	return 0;
}

RenderPassInfo createRenderPass(const WindowInfo& w) {
	VkRenderPass r;
	VkAttachmentDescription attachdescs[2] {{
			0, 
			GH_SWAPCHAIN_IMAGE_FORMAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		}, {
			0, 
			GH_DEPTH_BUFFER_IMAGE_FORMAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
	}};
	VkAttachmentReference attachrefs[2] {
		{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
		{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}
	};
	GH::createRenderPass(r, 2, &attachdescs[0], &attachrefs[0], nullptr, &attachrefs[1]);

	return RenderPassInfo(r, w.getNumSCIs(), w.getSCImages(), nullptr, w.getDepthBuffer(), {{0.3, 0.3, 0.3, 1}, {1, 0}});
}

PipelineInfo createViewportPipeline(const VkExtent2D& e, const VkRenderPass& r) {
	/* if ever there was a time to try no UV, this is it */
	PipelineInfo p;
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "viewport";
	p.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePCData)};
	p.objpushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, sizeof(ScenePCData), sizeof(MeshPCData)};
	p.vertexinputstateci = Mesh::getVISCI(VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL);
	p.depthtest = true;
	p.extent = e;
	p.renderpass = r;
	GH::createPipeline(p);
	Mesh::ungetVISCI(p.vertexinputstateci);
	return p;
}

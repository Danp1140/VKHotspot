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
	ramp.setScale(glm::vec3(0.5));
	Mesh povcube("../resources/models/cube.obj");

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

	tprp.addPipeline(tpp, &tps.getCamera()->getVP());
	tps.addRenderPass(tprp);
	tps.getRenderPass(0).addMesh(&floor, VK_NULL_HANDLE, &floor.getModelMatrix(), 0);
	tps.getRenderPass(0).addMesh(&ramp, VK_NULL_HANDLE, &ramp.getModelMatrix(), 0);
	tps.getRenderPass(0).addMesh(&povcube, VK_NULL_HANDLE, &povcube.getModelMatrix(), 0);
	tps.getCamera()->setFOVY(glm::half_pi<float>());

	fpw.addTasks(fps.getDrawTasks());
	tpw.addTasks(tps.getDrawTasks());

	/*
	 * Setting Up Physics Objects
	 */
	
	/* TODO: error when we swap order of adding pov and deathplane */
	PhysicsHandler ph;
	PointCollider* pov = static_cast<PointCollider*>(ph.addCollider(PointCollider()));
	pov->setPos(glm::vec3(0, 5, 0));
	pov->applyForce(glm::vec3(0, -9.807 * pov->getMass(), 0));

	PlaneCollider* deathplane = static_cast<PlaneCollider*>(ph.addCollider(PlaneCollider(glm::vec3(0, 1, 0))));
	deathplane->setMass(std::numeric_limits<float>::infinity());
	deathplane->setPos(glm::vec3(0, -1, 0));

	RectCollider* mainstage = static_cast<RectCollider*>(ph.addCollider(RectCollider(glm::vec3(0, 1, 0), glm::vec2(10))));
	mainstage->setMass(std::numeric_limits<float>::infinity());

	RectCollider* rampcol = static_cast<RectCollider*>(ph.addCollider(RectCollider(glm::vec3(0, 1, 1), glm::vec2(5))));
	rampcol->setMass(std::numeric_limits<float>::infinity());
	// TODO: figure out why this is inconsistent w/ visual position of collider
	rampcol->setPos(glm::vec3(0, 5 / glm::root_two<float>(), -5));
	
	ph.addColliderPair(ColliderPair(pov, deathplane));
	ph.addColliderPair(ColliderPair(pov, mainstage));
	ph.addColliderPair(ColliderPair(pov, rampcol));

	ph.getColliderPair(0).setOnCollide([] (void* d) {
			Collider* c = static_cast<Collider*>(d);
			c->setPos(glm::vec3(0, 5, 0));
		}, pov);
	ph.getColliderPair(0).setPreventDefault(true);

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

	SDL_Event eventtemp;
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
		fps.getCamera()->setPos(pov->getPos() + glm::vec3(0, 1, 0));
		povcube.setPos(pov->getPos() + glm::vec3(0, 1, 0));
		ramp.setPos(rampcol->getPos());
		ramp.setRot(rampcol->getRot());
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
	GH::createRenderPass(r, 2, &attachdescs[0], &attachrefs[0], &attachrefs[1]);

	return RenderPassInfo(r, w.getNumSCIs(), w.getSCImages(), w.getDepthBuffer(), {{0.3, 0.3, 0.3, 1}, {1, 0}});
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

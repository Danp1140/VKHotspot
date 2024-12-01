#include "Scene.h"
#include "PhysicsHandler.h"
#include "InputHandler.h"

#define MOVEMENT_SENS 2.f
#define CAMERA_SENS 0.01f

RenderPassInfo createRenderPass(const WindowInfo& w);

PipelineInfo createViewportPipeline(const VkExtent2D& e, const VkRenderPass& r);

int main() {
	GH gh;
	WindowInfo fpw(glm::vec2(0, 0), glm::vec2(0.5, 1)), 
		tpw(glm::vec2(0.5, 0), glm::vec2(0.5, 1));
	InputHandler ih;

	Mesh floor("../resources/models/plane.obj");
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

	tprp.addPipeline(tpp, &tps.getCamera()->getVP());
	tps.addRenderPass(tprp);
	tps.getRenderPass(0).addMesh(&floor, VK_NULL_HANDLE, &floor.getModelMatrix(), 0);
	tps.getRenderPass(0).addMesh(&povcube, VK_NULL_HANDLE, &povcube.getModelMatrix(), 0);

	fpw.addTasks(fps.getDrawTasks());
	tpw.addTasks(tps.getDrawTasks());

	/*
	 * Setting Up Physics Objects
	 */
	
	/* TODO: error when we swap order of adding pov and deathplane */
	PhysicsHandler ph;
	PointCollider* pov = static_cast<PointCollider*>(ph.addCollider(PointCollider()));
	pov->setPos(glm::vec3(0, 10, 0));
	pov->applyForce(glm::vec3(0, -9.807 * pov->getMass(), 0));

	PlaneCollider* deathplane = static_cast<PlaneCollider*>(ph.addCollider(PlaneCollider(glm::vec3(0, 1, 0))));
	deathplane->setMass(std::numeric_limits<float>::infinity());

	RectCollider* mainstage = static_cast<RectCollider*>(ph.addCollider(RectCollider(glm::vec3(0, 1, 0), glm::vec2(20))));
	
	// ph.addColliderPair(ColliderPair(pov, deathplane));

	ih.addHold(InputHold(
		[] (const SDL_Event& e) { return e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_W; },
		[] (const SDL_Event& e) { return e.type == SDL_EVENT_KEY_UP && e.key.scancode == SDL_SCANCODE_W; },
		[&ph, pov, c = fps.getCamera()] () { ph.addTimedMomentum({pov, MOVEMENT_SENS * glm::normalize(c->getForward() * glm::vec3(1, 0, 1)), 0}); }));
	ih.addHold(InputHold(SDL_SCANCODE_A, [&ph, pov, c = fps.getCamera()] () { ph.addTimedMomentum({pov, -MOVEMENT_SENS * c->getRight(), 0}); }));
	ih.addHold(InputHold(SDL_SCANCODE_S, [&ph, pov, c = fps.getCamera()] () { ph.addTimedMomentum({pov, -MOVEMENT_SENS * glm::normalize(c->getForward() * glm::vec3(1, 0, 1)), 0}); }));
	ih.addHold(InputHold(SDL_SCANCODE_D, [&ph, pov, c = fps.getCamera()] () { ph.addTimedMomentum({pov, MOVEMENT_SENS * c->getRight(), 0}); }));

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
		ih.update();
		SDL_PumpEvents();

		ph.update();
		fps.getCamera()->setPos(pov->getPos() + glm::vec3(0, 1, 0));
		povcube.setPos(pov->getPos() + glm::vec3(0, 1, 0));

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
	p.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePCData) + sizeof(MeshPCData)};
	p.vertexinputstateci = Mesh::getVISCI(VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL);
	p.depthtest = true;
	p.extent = e;
	p.renderpass = r;
	GH::createPipeline(p);
	Mesh::ungetVISCI(p.vertexinputstateci);
	return p;
}

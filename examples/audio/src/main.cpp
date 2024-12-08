#include "AudioHandler.h"
#include "Scene.h"

RenderPassInfo createRenderPass(const WindowInfo& w);

PipelineInfo createViewportPipeline(const VkExtent2D& e, const VkRenderPass& r);

void setupStereoExample(AudioHandler& a);

int main() {
	AudioHandler ah;

	GH gh;
	WindowInfo w;
	RenderPassInfo rp = createRenderPass(w);
	PipelineInfo p = createViewportPipeline(w.getSCExtent(), rp.getRenderPass());
	Scene s((float)w.getSCExtent().width / (float)w.getSCExtent().height);
	rp.addPipeline(p, &s.getCamera()->getVP());
	s.addRenderPass(rp);

	Mesh soundmesh("../resources/models/sound.obj");
	s.getRenderPass(0).addMesh(&soundmesh, VK_NULL_HANDLE, &soundmesh.getModelMatrix(), 0);
	w.addTasks(s.getDrawTasks());

	ah.addSound(Sound());
	ah.getSound(0).setPos(glm::vec3(0, 0, -10));
	ah.getSound(0).setForward(glm::vec3(0, 0, 1));
	ah.getSound(0).setRespFunc(AudioObject::hemisphericalResponseFunction);
	ah.addListener(Listener());

	soundmesh.setPos(ah.getSound(0).getPos());
	s.getCamera()->setPos(ah.getListener(0).getPos());
	s.getCamera()->setForward(ah.getListener(0).getForward());

	ah.start();
	float theta = 0;
	const float scale = 100;
	while (w.frameCallback()) {
		SDL_PumpEvents();

		// Tornado Siren
		// ah.getSound(0).setForward(glm::vec3(sin(theta), 0, cos(theta)));

		// Ambulance Donuts
		// ah.getSound(0).setPos(scale * glm::vec3(sin(theta), 0, cos(theta) - 1) + glm::vec3(0, 0, -5));

		soundmesh.setPos(ah.getSound(0).getPos());
		glm::vec3 v = glm::cross(glm::vec3(0, 0, 1), ah.getSound(0).getForward());
		soundmesh.setRot(glm::normalize(glm::quat(1 + glm::dot(glm::vec3(0, 0, 1), ah.getSound(0).getForward()), v)));
		theta += 0.01;
	}

	// Pa_Sleep(20000);

	// Ambulance Donuts Test
	/*
	ah.getSound(0).setRespFunc(AudioObject::sphericalResponseFunction);
	float theta = 0;
	float scale = 100;
	for (uint8_t i = 0; i < UINT8_MAX; i++) {
		ah.getSound(0).setPos(scale * glm::vec3(sin(theta), 0, cos(theta) - 1);
		theta += 0.01;
		Pa_Sleep(17);
	}
	*/

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

void setupStereoExample(AudioHandler& a) {
	ah.addListener(Listener());
}

#include "AudioHandler.h"
#include "Scene.h"
#include "InputHandler.h"

#define CAMERA_SENS 0.01f

RenderPassInfo createRenderPass(const WindowInfo& w);

PipelineInfo createViewportPipeline(const VkExtent2D& e, const VkRenderPass& r);

PipelineInfo createInstancedPipeline(const VkExtent2D& e, const VkRenderPass& r);

void setupStereoExample(AudioHandler& a);

void updateStereoExample(AudioHandler& a, Scene& s);

void makeAudioMeshes(const AudioHandler& a, InstancedMesh* dst, Scene& s);

float conicResponseFunction(const glm::vec3& f, const glm::vec3& r) {
	const float c = 0.5, s = 20, b = 0.5;
	return (1 / (1 + exp(-s * (glm::dot(f, glm::normalize(r)) - c))) + b) / (1 - b);
}

int main() {
	AudioHandler ah;

	GH gh;
	WindowInfo w;
	RenderPassInfo rp = createRenderPass(w);
	PipelineInfo p = createInstancedPipeline(w.getSCExtent(), rp.getRenderPass());
	Scene s((float)w.getSCExtent().width / (float)w.getSCExtent().height);
	rp.addPipeline(p, &s.getCamera()->getVP());
	s.addRenderPass(rp);

	setupStereoExample(ah);

	/*
	Mesh soundmesh("../resources/models/sound.obj");
	s.getRenderPass(0).addMesh(&soundmesh, VK_NULL_HANDLE, &soundmesh.getModelMatrix(), 0);
	*/
	InstancedMesh* soundsmesh;
	makeAudioMeshes(ah, soundsmesh, s);
	w.addTasks(s.getDrawTasks());

	/*
	ah.addSound(Sound("../resources/sounds/ta1.1mono.wav"));
	ah.getSound(0).setPos(glm::vec3(0, 0, -10));
	ah.getSound(0).setForward(glm::vec3(0, 0, 1));
	ah.getSound(0).setRespFunc(AudioObject::hemisphericalResponseFunction);
	ah.addListener(Listener());
	*/

	/*
	soundmesh.setPos(ah.getSound(0).getPos());
	*/
	s.getCamera()->setPos(glm::vec3(0));
	s.getCamera()->setForward(glm::vec3(0, 0, -1));

	InputHandler ih;
	ih.addCheck(InputCheck(SDL_EVENT_MOUSE_MOTION, [c = s.getCamera()] (const SDL_Event& e) {
		c->setForward(c->getForward() + CAMERA_SENS * (c->getRight() * e.motion.xrel + c->getUp() * -e.motion.yrel));
		return true;
	}));


	ah.start();
	float theta = 0;
	const float scale = 100;
	while (w.frameCallback()) {
		ih.update();
		SDL_PumpEvents();

		updateStereoExample(ah, s);

		// Tornado Siren
		// ah.getSound(0).setForward(glm::vec3(sin(theta), 0, cos(theta)));

		// Ambulance Donuts
		// ah.getSound(0).setPos(scale * glm::vec3(sin(theta), 0, cos(theta) - 1) + glm::vec3(0, 0, -5));

		/*
		soundmesh.setPos(ah.getSound(0).getPos());
		glm::vec3 v = glm::cross(glm::vec3(0, 0, 1), ah.getSound(0).getForward());
		soundmesh.setRot(glm::normalize(glm::quat(1 + glm::dot(glm::vec3(0, 0, 1), ah.getSound(0).getForward()), v)));
		*/
		// TODO: update generated audio meshes
		theta += 0.01;
	}

	vkQueueWaitIdle(GH::getGenericQueue());

	delete soundsmesh;

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

PipelineInfo createInstancedPipeline(const VkExtent2D& e, const VkRenderPass& r) {
	PipelineInfo p;
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "instanced";
	VkDescriptorSetLayoutBinding bindings[1] {{
		0,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		1,
		VK_SHADER_STAGE_VERTEX_BIT,
		nullptr
	}};
	p.descsetlayoutci = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1, &bindings[0]
	};
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
	a.addListener(Listener({1, 0}));
	a.addListener(Listener({0, 1}));
	a.getListener(0).setRespFunc(conicResponseFunction);
	a.getListener(1).setRespFunc(conicResponseFunction);
	a.getListener(0).setForward(glm::vec3(-0.5, 0, -1));
	a.getListener(1).setForward(glm::vec3(0.5, 0, -1));

	a.addSound(Sound("../resources/sounds/ta1.1mono.wav"));
	a.addSound(Sound("../resources/sounds/ta2.1mono.wav"));
	a.addSound(Sound("../resources/sounds/eu1.1mono.wav"));
	a.addSound(Sound("../resources/sounds/eu2.1mono.wav"));
	a.addSound(Sound("../resources/sounds/eu3.1mono.wav"));
	a.addSound(Sound("../resources/sounds/ml1.1mono.wav"));
	a.addSound(Sound("../resources/sounds/ml2.1mono.wav"));
	a.addSound(Sound("../resources/sounds/ml3.1mono.wav"));

	const float r = 10;
	const float dtheta = 6.28;
	float theta = -dtheta / 2;
	for (uint8_t i = 0; i < a.getSounds().size(); i++) {
		a.getSound(i).setPos(r * glm::vec3(sin(theta), 0, cos(theta)));
		a.getSound(i).setForward(glm::vec3(-sin(theta), 0, -cos(theta)));
		theta += dtheta / a.getSounds().size();
	}
	/*
	a.getSound(0).setPos(glm::vec3(5, 0, -10));
	a.getSound(1).setPos(glm::vec3(-5, 0, -10));
	a.getSound(2).setPos(glm::vec3(10, 0, -10));
	a.getSound(3).setPos(glm::vec3(-10, 0, -10));
	a.getSound(4).setPos(glm::vec3(-15, 0, -10));
	a.getSound(0).setForward(glm::vec3(-5, 0, 10));
	a.getSound(1).setForward(glm::vec3(5, 0, 10));
	a.getSound(2).setForward(glm::vec3(-10, 0, 10));
	a.getSound(3).setForward(glm::vec3(10, 0, 10));
	a.getSound(4).setForward(glm::vec3(15, 0, 10));
	*/
	// TODO: conic resp func for horns too later
}

void updateStereoExample(AudioHandler& a, Scene& s) {
	a.getListener(0).setForward(s.getCamera()->getForward() - 0.5f * s.getCamera()->getRight());
	a.getListener(1).setForward(s.getCamera()->getForward() + 0.5f * s.getCamera()->getRight());
}

void makeAudioMeshes(const AudioHandler& a, InstancedMesh* dst, Scene& s) {
	std::vector<InstancedMeshData> imd;
	glm::vec3 v;
	for (Sound* s : a.getSounds()) {
		v = glm::cross(glm::vec3(0, 0, 1), s->getForward());
		imd.push_back({
			glm::translate(glm::mat4(1), s->getPos()) 
			 * glm::mat4_cast(glm::normalize(glm::quat(1 + glm::dot(glm::vec3(0, 0, 1), s->getForward()), v)))});
	}
	dst = new InstancedMesh("../resources/models/sound.obj", imd);

	VkDescriptorSet ds;
	GH::createDS(s.getRenderPass(0).getRenderSet(0).pipeline, ds);
	GH::updateDS(ds, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {}, dst->getInstanceUB().getDBI());

	s.getRenderPass(0).addMesh(dst, ds, nullptr, 0);
}


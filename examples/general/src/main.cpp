#include "UIHandler.h"
#include "Scene.h"
#include "PhysicsHandler.h"
#include "TextureHandler.h"

void createScene(Scene& s, const WindowInfo& w, const Mesh& m) {
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

	RenderPassInfo rpi(r, w.getNumSCIs(), w.getSCImages(), w.getDepthBuffer(), {{0.3, 0.3, 0.3, 1}, {1, 0}});

	PipelineInfo p;
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "default";
	p.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePCData) + sizeof(MeshPCData)};
	p.vertexinputstateci = Mesh::getVISCI(VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL);
	p.depthtest = true;
	p.extent = w.getSCExtent();
	p.renderpass = r;
	GH::createPipeline(p);
	rpi.addPipeline(p, &s.getCamera()->getVP());
	Mesh::ungetVISCI(p.vertexinputstateci);
	
	rpi.addMesh(&m, VK_NULL_HANDLE, &m.getModelMatrix(), 0);

	PipelineInfo ip;
	ip.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	ip.shaderfilepathprefix = "instanced";
	VkDescriptorSetLayoutBinding bindings[1] {{
		0,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		1,
		VK_SHADER_STAGE_VERTEX_BIT,
		nullptr
	}};
	ip.descsetlayoutci = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1, &bindings[0]
	};
	ip.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePCData)};
	ip.vertexinputstateci = Mesh::getVISCI(VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL);
	ip.depthtest = true;
	ip.extent = w.getSCExtent();
	ip.renderpass = r;
	GH::createPipeline(ip);
	rpi.addPipeline(ip, &s.getCamera()->getVP());
	Mesh::ungetVISCI(ip.vertexinputstateci);

	PipelineInfo tp;
	tp.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	tp.shaderfilepathprefix = "diffusetexture";
	VkDescriptorSetLayoutBinding dtbindings[1] {{
		0,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	}};
	tp.descsetlayoutci = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1, &dtbindings[0]
	};
	tp.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePCData)};
	tp.vertexinputstateci = Mesh::getVISCI(VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL);
	tp.depthtest = true;
	tp.extent = w.getSCExtent();
	tp.renderpass = r;
	GH::createPipeline(tp);
	rpi.addPipeline(tp, &s.getCamera()->getVP());
	Mesh::ungetVISCI(tp.vertexinputstateci);

	s.addRenderPass(rpi);
}

InstancedMesh createCubeRing(std::vector<InstancedMeshData>& d, uint32_t steps, float r) {
	d.resize(steps);
	float theta;
	for (uint32_t i = 0; i < steps; i++) {
		theta = (float)i / (float)steps * glm::two_pi<float>();
		d[i].m = glm::mat4(1);
		d[i].m = glm::translate<float>(d[i].m, r * glm::vec3(sin(theta), 0, cos(theta)));
		d[i].m = glm::rotate<float>(d[i].m, theta, glm::vec3(0, 1, 0));
		d[i].m = glm::scale<float>(d[i].m, glm::vec3(0.1));
	}
	return InstancedMesh("../resources/models/cube.obj", d);
}

void throbCubeRing(InstancedMesh& m, std::vector<InstancedMeshData>& d, float frequency, float t) {
	std::vector<InstancedMeshData> res(d.size());
	const float theta1 = t * frequency * glm::two_pi<float>();
	float theta2;
	for (uint32_t i = 0; i < d.size(); i++) {
		theta2 = (float)i / (float)d.size() * glm::two_pi<float>();
		res[i].m = glm::translate<float>(d[i].m, 5.f * glm::vec3(0, glm::sech(5 * (fmod(theta1 + theta2, glm::two_pi<float>()) - glm::pi<float>())), 0));
	}
	m.updateInstanceUB(res);
}

int main() {
	GH graphicshandler = GH();
	WindowInfo w;
	UIHandler ui(w.getSCExtent());
	TextureHandler th;

	/*
	w.addTask(cbRecTaskTemplate(cbRecTaskRenderPassTemplate(
		ui.getRenderPass(),
		w.getPresentationFBs(),
		w.getSCImages()[0].extent,
		1, &ui.getColorClear())));
	// TODO: fix UIHandler, this is disgusting lol [l] 
	w.addTask(cbRecTaskTemplate(cbRecFuncTemplate(
		VK_NULL_HANDLE,
		nullptr,
		VK_NULL_HANDLE, VK_NULL_HANDLE,
		VK_NULL_HANDLE, VK_NULL_HANDLE,
		[&ui, &w] (cbRecData d, VkCommandBuffer& cb) { ui.draw(cb, w.getCurrentPresentationFB()); },
		w.getPresentationFBs())));
	ui.addComponent(UIText(L"text from main", UICoord(1000, 1000)));
	*/
	
	Scene s((float)w.getSCExtent().width / (float)w.getSCExtent().height);
	Mesh m("../resources/models/plane.obj");
	createScene(s, w, m);

	std::vector<InstancedMeshData> imdatatemp;
	InstancedMesh im = createCubeRing(imdatatemp, 32, 3);
	VkDescriptorSet temp;
	GH::createDS(s.getRenderPass(0).getRenderSet(1).pipeline, temp);
	GH::updateDS(temp, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {}, im.getInstanceUB().getDBI());
	s.getRenderPass(0).addMesh(&im, temp, nullptr, 1);

	Mesh plane("../resources/models/plane.obj");
	s.getRenderPass(0).addMesh(&plane, VK_NULL_HANDLE, &plane.getModelMatrix(), 0);

	Mesh suz("../resources/models/suzanne.obj");
	TextureSet t("../resources/textures/uvgrid");
	t.setDiffuseSampler(th.addSampler("bilinear", VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE));
	GH::createDS(s.getRenderPass(0).getRenderSet(2).pipeline, temp);
	GH::updateDS(temp, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, t.getDiffuse().getDII(), {});
	s.getRenderPass(0).addMesh(&suz, temp, &suz.getModelMatrix(), 2);

	w.addTasks(s.getDrawTasks());

	PhysicsHandler ph;

	PointCollider* pc = static_cast<PointCollider*>(ph.addCollider(PointCollider()));
	pc->setPos(glm::vec3(0, 5, -10));
	pc->applyForce(glm::vec3(0, -9.807, 5));

	/*
	MeshCollider* mc = static_cast<MeshCollider*>(ph.addCollider(MeshCollider("resources/models/objs/plane.obj")));
	mc->setMass(std::numeric_limits<float>::infinity());
	*/

	PlaneCollider* plc = static_cast<PlaneCollider*>(ph.addCollider(PlaneCollider(glm::vec3(0, 1, 0))));
	plc->setMass(std::numeric_limits<float>::infinity());

	ph.addColliderPair(ColliderPair(pc, plc));

	ph.start();
	bool xpressed = false;
	SDL_Event eventtemp;
	while (!xpressed) {
		throbCubeRing(im, imdatatemp, 0.5, (float)SDL_GetTicks() / 1000);

		m.setPos(pc->getPos() + glm::vec3(0, 1, 0));
		plane.setPos(plc->getPos());

		w.frameCallback();
		
		SDL_Delay(17);
		ph.update();

		while (SDL_PollEvent(&eventtemp)) {
			if (eventtemp.type == SDL_EVENT_QUIT
				|| eventtemp.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
				xpressed = true;
			}
		}
	}
	vkQueueWaitIdle(GH::getGenericQueue());

	return 0;
}

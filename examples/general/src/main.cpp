#include "Scene.h"
#include "PhysicsHandler.h"
#include "TextureHandler.h"
#include "InputHandler.h"

#define MOVEMENT_SENS 0.75f
#define FOV_SENS 0.05f

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
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
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
	p.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePCData)};
	p.objpushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, sizeof(ScenePCData), sizeof(MeshPCData)};
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
	ip.objpushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, sizeof(ScenePCData), sizeof(MeshPCData)};
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
	tp.objpushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, sizeof(ScenePCData), sizeof(MeshPCData)};
	tp.vertexinputstateci = Mesh::getVISCI(VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL);
	tp.depthtest = true;
	tp.extent = w.getSCExtent();
	tp.renderpass = r;
	GH::createPipeline(tp);
	rpi.addPipeline(tp, &s.getCamera()->getVP());
	Mesh::ungetVISCI(tp.vertexinputstateci);
	s.addRenderPass(rpi);

	VkAttachmentDescription uiattachdesc {
		0,
		GH_SWAPCHAIN_IMAGE_FORMAT,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_LOAD,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};
	VkAttachmentReference uiattachref {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	GH::createRenderPass(r, 1, &uiattachdesc, &uiattachref, nullptr);
	rpi = RenderPassInfo(r, w.getNumSCIs(), w.getSCImages(), nullptr, {{0, 0, 0, 1}});

	PipelineInfo uip;
	uip.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	uip.shaderfilepathprefix = "UI";
	uip.renderpass = r;
	uip.extent = w.getSCExtent(); 
	uip.cullmode = VK_CULL_MODE_NONE;
	uip.pushconstantrange = {
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof(UIPushConstantData)
	};
	VkDescriptorSetLayoutBinding uipbindings[1] {{
		0,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	}};
	uip.descsetlayoutci = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1, &uipbindings[0]
	};
	VkSpecializationMapEntry specmap[2] {
		{0, 0, sizeof(uint32_t)},
		{1, sizeof(uint32_t), sizeof(uint32_t)}
	};
	uip.specinfo = {2, &specmap[0], sizeof(VkExtent2D), static_cast<void*>(&uip.extent)};
	GH::createPipeline(uip);
	rpi.addPipeline(uip, nullptr);
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

typedef struct LODFuncData {
	Camera* c;
	float min, max;
} LODFuncData;

bool suzDrawCond(Mesh& m, void* d) {
	LODFuncData* md = static_cast<LODFuncData*>(d);
	float dist = glm::distance(m.getPos(), md->c->getPos());
	return dist < md->max && dist > md->min;
}

LODMesh createLODSuzanne(Scene& s, std::vector<LODFuncData>& datadst) {
	std::vector<LODMeshData> datatemp;
	const float locutoffdist = 10, hicutoffdist = 75;
	datadst = std::vector<LODFuncData>();
	datadst.push_back((LODFuncData){s.getCamera(), 0, locutoffdist});
	datadst.push_back((LODFuncData){s.getCamera(), locutoffdist, hicutoffdist});
	datadst.push_back((LODFuncData){s.getCamera(), hicutoffdist, std::numeric_limits<float>::infinity()});
	datatemp.emplace_back(Mesh("../resources/models/suzannehi.obj"), [] (Mesh& m, void* d) {return true;}, suzDrawCond, nullptr, &datadst[0]);
	datatemp.emplace_back(Mesh("../resources/models/suzannemid.obj"), [] (Mesh& m, void* d) {return true;}, suzDrawCond, nullptr, &datadst[1]);
	datatemp.emplace_back(Mesh("../resources/models/suzannelo.obj"), [] (Mesh& m, void* d) {return true;}, suzDrawCond, nullptr, &datadst[2]);
	return LODMesh(datatemp);
}

int main() {
	GH graphicshandler = GH();
	WindowInfo w;
	TextureHandler th;

	Scene s((float)w.getSCExtent().width / (float)w.getSCExtent().height);
	Mesh m("../resources/models/cube.obj");
	createScene(s, w, m);

	UIHandler ui(s.getRenderPass(1).getRenderSet(0).pipeline, w.getSCExtent());
	UIContainer ct;
	ct.setPos(UICoord(0, w.getSCExtent().height));
	ct.setExt(UICoord(1000, -w.getSCExtent().height));
	// ct.addChild(UIText(L"text from main AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", UICoord(0, w.getSCExtent().height)));
	// ct.addChild(UIText(L"text from main AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", UICoord(100, 100)));
	// ui.addComponent(ct);
	ui.addComponent(UIText(L"text from main AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", UICoord(100, 1000)));
	s.getRenderPass(1).setUI(&ui, 0);

	std::vector<InstancedMeshData> imdatatemp;
	InstancedMesh im = createCubeRing(imdatatemp, 32, 3);
	VkDescriptorSet temp;
	GH::createDS(s.getRenderPass(0).getRenderSet(1).pipeline, temp);
	GH::updateDS(temp, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {}, im.getInstanceUB().getDBI());
	s.getRenderPass(0).addMesh(&im, temp, nullptr, 1);

	Mesh plane("../resources/models/plane.obj");
	s.getRenderPass(0).addMesh(&plane, VK_NULL_HANDLE, &plane.getModelMatrix(), 0);

	std::vector<LODFuncData> tempfd;
	LODMesh suz = createLODSuzanne(s, tempfd);
	TextureSet t("../resources/textures/uvgrid");
	t.setDiffuseSampler(th.addSampler("bilinear", VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE));
	GH::createDS(s.getRenderPass(0).getRenderSet(2).pipeline, temp);
	GH::updateDS(temp, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, t.getDiffuse().getDII(), {});
	s.getRenderPass(0).addMesh(&suz, temp, &suz.getModelMatrix(), 2);

	w.addTasks(s.getDrawTasks());

	PhysicsHandler ph;

	PointCollider* pc = static_cast<PointCollider*>(ph.addCollider(PointCollider()));
	pc->setPos(glm::vec3(0, 10, 0));
	// pc->applyForce(glm::vec3(0, -9.807, 0));

	PlaneCollider* plc = static_cast<PlaneCollider*>(ph.addCollider(PlaneCollider(glm::vec3(0, 1, 0))));
	plc->setMass(std::numeric_limits<float>::infinity());

	ph.addColliderPair(ColliderPair(pc, plc));

	InputHandler ih;
	glm::vec3 movementdir;
	ih.addHold(InputHold(SDL_SCANCODE_W, [&movementdir, c = s.getCamera()] () { movementdir += glm::normalize(c->getUp()); }));
	ih.addHold(InputHold(SDL_SCANCODE_A, [&movementdir, c = s.getCamera()] () { movementdir -= glm::normalize(c->getRight()); }));
	ih.addHold(InputHold(SDL_SCANCODE_S, [&movementdir, c = s.getCamera()] () { movementdir -= glm::normalize(c->getUp()); }));
	ih.addHold(InputHold(SDL_SCANCODE_D, [&movementdir, c = s.getCamera()] () { movementdir += glm::normalize(c->getRight()); }));
	ih.addHold(InputHold(SDL_SCANCODE_E, [&movementdir, c = s.getCamera()] () { movementdir += glm::normalize(c->getForward()); }));
	ih.addHold(InputHold(SDL_SCANCODE_Q, [&movementdir, c = s.getCamera()] () { movementdir -= glm::normalize(c->getForward()); }));
	ih.addHold(InputHold(SDL_SCANCODE_UP, [&movementdir, c = s.getCamera()] () { if (c->getFOVY() > FOV_SENS) c->setFOVY(c->getFOVY() - FOV_SENS); }));
	ih.addHold(InputHold(SDL_SCANCODE_DOWN, [&movementdir, c = s.getCamera()] () { if (c->getFOVY() < glm::pi<float>() - FOV_SENS) c->setFOVY(c->getFOVY() + FOV_SENS); }));

	ph.start();
	bool xpressed = false;
	SDL_Event eventtemp;
	while (w.frameCallback()) {
		movementdir = glm::vec3(0);
		ih.update();
		SDL_PumpEvents();
		if (movementdir != glm::vec3(0)) {
			s.getCamera()->setPos(s.getCamera()->getPos() + MOVEMENT_SENS * glm::normalize(movementdir));
			s.getCamera()->setForward(-s.getCamera()->getPos());
		}

		throbCubeRing(im, imdatatemp, 0.5, (float)SDL_GetTicks() / 1000);

		m.setPos(pc->getPos() + glm::vec3(0, 1, 0));
		plane.setPos(plc->getPos());

		SDL_Delay(17); // TODO: get rid of this delay lol
		ph.update();
	}
	vkQueueWaitIdle(GH::getGenericQueue());

	return 0;
}

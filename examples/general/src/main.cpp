#include "Scene.h"
#include "PhysicsHandler.h"
#include "TextureHandler.h"
#include "InputHandler.h"
#include "PostProcessing.h"
#include <random>

#define MOVEMENT_SENS 0.75f
#define FOV_SENS 0.05f

typedef struct [[gnu::packed]] DNSScenePCData {
	glm::mat4 vp;
	glm::vec3 c_pos;
} DNSScenePCData;

typedef struct [[gnu::packed]] DNSObjectPCData {
	uint32_t catcher_idx;
	glm::mat4 m;
} DNSObjectPCData;

typedef struct POMPCData {
	glm::mat4 vp;
	glm::vec4 c_p;
} POMPCData;

PipelineInfo createSMPipelineTemplate(RenderPassInfo* rpi) {
	PipelineInfo p;
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "shadowmap";
	p.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)};
	p.objpushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), sizeof(glm::mat4)};
	p.vertexinputstateci = Mesh::getVISCI(VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL, VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL);
	p.depthtest = true;
	// p.cullmode = VK_CULL_MODE_FRONT_BIT;
	p.cullmode = VK_CULL_MODE_NONE;
	p.dyn_viewport = true;
	p.renderpass = rpi->getRenderPass();
	GH::createPipeline(p);
	Mesh::ungetVISCI(p.vertexinputstateci);
	return p;
}

size_t createShadowReceivePipeline(Scene& s, const WindowInfo& w, RenderPassInfo* rpi) {
	PipelineInfo p;
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "shadowtest";
	VkDescriptorSetLayoutBinding bindings[3] {{
			0,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			1,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		}, {
			1,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		}, {
			2,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			1,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr

	}};
	p.descsetlayoutci = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		3, &bindings[0]
	};
	p.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DNSScenePCData)};
	p.objpushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(DNSScenePCData), sizeof(DNSObjectPCData)};
	p.vertexinputstateci = Mesh::getVISCI(VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL);
	p.depthtest = true;
	p.extent = w.getSCExtent();
	p.msaasamples = w.getMSAASamples();
	p.renderpass = rpi->getRenderPass();
	GH::createPipeline(p);
	size_t res = rpi->addPipeline(p, &s.getCamera()->getVP());
	Mesh::ungetVISCI(p.vertexinputstateci);
	return res;
}

RenderPassInfo* createSMRenderPass(Scene& s, const WindowInfo& w) {
	VkRenderPass r;
	VkAttachmentDescription a_d {
		0, 
		LIGHT_SHADOW_MAP_FORMAT,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};
	VkAttachmentReference a_r {0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
	GH::createRenderPass(r, 1, &a_d, nullptr, nullptr, &a_r);
	RenderPassInfo rpi(r, 1, nullptr, nullptr, &s.getShadowAtlas(), {{1, 0}});

	return s.addRenderPass(rpi);
}

RenderPassInfo* createMainRP(Scene& s, const WindowInfo& w, PPStep& pproc) {
	VkRenderPass r;
	VkAttachmentDescription attachdescs[4] {{
			0, 
			GH_SWAPCHAIN_IMAGE_FORMAT,
			w.getMSAASamples(),
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		}, {
			0, 
			GH_DEPTH_BUFFER_IMAGE_FORMAT,
			w.getMSAASamples(),
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		}, {
			0, 
			GH_SWAPCHAIN_IMAGE_FORMAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
		}, {
			0, 
			GH_DEPTH_BUFFER_IMAGE_FORMAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	}};
	VkAttachmentReference attachrefs[4] {
		{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
		{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
		{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
		{3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}
	};
	GH::createRenderPass(r, 4, &attachdescs[0], &attachrefs[0], &attachrefs[2], &attachrefs[1]);
	// RenderPassInfo rpi(r, w.getNumSCIs(), w.getSCImages(), &w.getMSAAImage(), w.getDepthBuffer(), {{0.3, 0.3, 0.3, 1}, {1, 0}});
	std::vector<const ImageInfo*> att_imgs = {&w.getMSAAImage(), w.getDepthBuffer(), w.getSCImages(), &pproc.getDepthRes()};
	return s.addRenderPass(RenderPassInfo(r, w.getNumSCIs(), w.getMSAAImage().extent, {{0.3, 0.3, 0.3, 1}, {1, 0}}, att_imgs, 2));
}

size_t createDefaultPipeline(Scene& s, const WindowInfo& w, RenderPassInfo* rpi) {
	PipelineInfo p;
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "default";
	p.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePCData)};
	p.objpushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, sizeof(ScenePCData), sizeof(MeshPCData)};
	p.vertexinputstateci = Mesh::getVISCI(VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL);
	p.depthtest = true;
	p.extent = w.getSCExtent();
	p.msaasamples = w.getMSAASamples();
	p.renderpass = rpi->getRenderPass();
	GH::createPipeline(p);
	size_t res = rpi->addPipeline(p, &s.getCamera()->getVP());
	Mesh::ungetVISCI(p.vertexinputstateci);
	return res;
}

size_t createInstancedPipeline(Scene& s, const WindowInfo& w, RenderPassInfo* rpi) {
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
	ip.msaasamples = w.getMSAASamples();
	ip.renderpass = rpi->getRenderPass();
	GH::createPipeline(ip);
	size_t res = rpi->addPipeline(ip, &s.getCamera()->getVP());
	Mesh::ungetVISCI(ip.vertexinputstateci);
	return res;
}

size_t createTexturedPipeline(Scene& s, const WindowInfo& w, RenderPassInfo* rpi) {
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
	tp.renderpass = rpi->getRenderPass();
	tp.msaasamples = w.getMSAASamples();
	GH::createPipeline(tp);
	size_t res = rpi->addPipeline(tp, &s.getCamera()->getVP());
	Mesh::ungetVISCI(tp.vertexinputstateci);
	return res;
}

size_t createPOMPipeline(Scene& s, const WindowInfo& w, RenderPassInfo* rpi) {
	PipelineInfo pomp;
	pomp.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pomp.shaderfilepathprefix = "ndp";
	/*
	VkDescriptorSetLayoutBinding dtbindings[1] {{
			0,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
	}};
	pomp.descsetlayoutci = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1, &dtbindings[0]
	};
	*/
	pomp.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(POMPCData)};
	pomp.objpushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, sizeof(POMPCData), sizeof(POMPCData)};
	pomp.vertexinputstateci = Mesh::getVISCI(VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL | VERTEX_BUFFER_TRAIT_TANGENT | VERTEX_BUFFER_TRAIT_BITANGENT);
	pomp.depthtest = true;
	pomp.extent = w.getSCExtent();
	pomp.renderpass = rpi->getRenderPass();
	pomp.msaasamples = w.getMSAASamples();
	GH::createPipeline(pomp);
	size_t res = rpi->addPipeline(pomp, nullptr);
	Mesh::ungetVISCI(pomp.vertexinputstateci);
	return res;
}

RenderPassInfo createUIRPI(const WindowInfo& w) {
	VkRenderPass r;
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
	GH::createRenderPass(r, 1, &uiattachdesc, &uiattachref, nullptr, nullptr);
	RenderPassInfo rpi = RenderPassInfo(r, w.getNumSCIs(), w.getSCImages(), nullptr, nullptr, {{0, 0, 0, 1}});

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
	VkSpecializationInfo spi[2];
	spi[0] = {2, &specmap[0], sizeof(VkExtent2D), static_cast<void*>(&uip.extent)};
	spi[1] = {.dataSize = 0};
	uip.specinfo = &spi[0];
	GH::createPipeline(uip);
	rpi.addPipeline(uip, nullptr);
	return rpi;
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
	return InstancedMesh("../../resources/models/objs/cube.obj", d);
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
	datatemp.emplace_back(Mesh("resources/models/suzannehi.obj"), [] (Mesh& m, void* d) {return true;}, suzDrawCond, nullptr, &datadst[0]);
	datatemp.emplace_back(Mesh("resources/models/suzannemid.obj"), [] (Mesh& m, void* d) {return true;}, suzDrawCond, nullptr, &datadst[1]);
	datatemp.emplace_back(Mesh("resources/models/suzannelo.obj"), [] (Mesh& m, void* d) {return true;}, suzDrawCond, nullptr, &datadst[2]);
	return LODMesh(datatemp);
}

void prependTwoDigitTime(SDL_Time t, std::wstring& s) {
	s.insert(0, std::to_wstring(t));
	if (t < 10) s.insert(0, L"0");
}

std::wstring getTimestamp() {
	SDL_Time t;
	SDL_GetCurrentTime(&t);
	t = SDL_NS_TO_SECONDS(t);
	SDL_Time temp = t % 60;
	std::wstring res;
	prependTwoDigitTime(temp, res);
	res.insert(0, L":");
	t /= 60;
	temp = t % 60;
	prependTwoDigitTime(temp, res);
	res.insert(0, L":");
	t /= 60;
	temp = t % 24;
	prependTwoDigitTime(temp, res);
	res.append(L" (UTC lol)");
	return res;
}

// TODO: economize updates in this function
void addLight(WindowInfo& w, Scene& s, MeshBase& suzanne, MeshBase& plane, PipelineInfo sm_pipeline, RenderPassInfo* rpi, size_t shadowreceive_pidx) {
	std::random_device rdev;
	std::mt19937 gen(rdev());
	std::uniform_real_distribution<float> thetadist(0, glm::two_pi<float>()),
		phidist(0, glm::half_pi<float>()),
		rdist(3, 10),
		coldist(0, 0.75);
	float theta = thetadist(gen),
		phi = phidist(gen),
		r = rdist(gen);
	DirectionalLightInitInfo l_ii;
	l_ii.super_light.c = glm::vec3(coldist(gen), coldist(gen), coldist(gen));
	l_ii.super_directional.f = glm::vec3(r * glm::vec3(cos(theta) * cos(phi), sin(phi), sin(theta) * cos(phi)));

	DirectionalLight* l = s.addDirectionalLight(DirectionalLight(l_ii), {{1024, 1024}});
	std::vector<size_t> sm_p_idxs = s.addSMPipeline(*l, sm_pipeline, *rpi, nullptr);
	for (size_t i = 0; i < sm_p_idxs.size(); i++) {
		rpi->setScenePC(sm_p_idxs[i], &l->getSMDatum(i).getVP()); 
		rpi->addMesh(&suzanne, VK_NULL_HANDLE, &suzanne.getModelMatrix(), sm_p_idxs[i]);
	}

	// technically inefficient to rewrite already written data here, but shouldn't happen often
	std::vector<uint32_t> idxs;
	for (uint32_t i = 0; i < s.getNumSCLights(); i++) idxs.push_back(i);
	const RenderSet& rs = rpi->getRenderSet(shadowreceive_pidx);
	// current state of affairs:
	// - hooking up just one in the first slot works great
	// - hooking up multiple works for the first one, the second appears distorted, and the third and rest black out everything
	// - since one at a time hookup works, it seems like its an issue with the CUB
	s.updateLightCatcher(&plane, rs.objdss[rs.findMesh(&plane)], idxs, {}, {}, 0);
	// s.updateLightCatcher(&plane, rs.objdss[rs.findMesh(&plane)], {}, {s.getNumDirSCLights() - 1}, 0);
	s.addShadowCaster(&suzanne, idxs);
}

int main() {
	GHInitInfo ghii;
	// shouldn't be required in vulkan 1.2+, but MoltenVK sucks
	ghii.dexts.push_back("VK_KHR_depth_stencil_resolve");
	ghii.dexts.push_back("VK_KHR_create_renderpass2");
	ghii.dexts.push_back("VK_KHR_multiview");
	ghii.dexts.push_back("VK_KHR_maintenance2");
	ghii.dexts.push_back("VK_KHR_uniform_buffer_standard_layout"); 
	ghii.dps = {};
	ghii.dps.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64});
	ghii.dps.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8});
	VkPhysicalDeviceUniformBufferStandardLayoutFeatures ubo_std_layout;
	ubo_std_layout.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES;
	ubo_std_layout.pNext = nullptr;
	ubo_std_layout.uniformBufferStandardLayout = VK_TRUE;
	ghii.pdfeats.pNext = &ubo_std_layout;

	GH graphicshandler = GH(ghii);
	WindowInfo w((WindowInitInfo){.msaa = VK_SAMPLE_COUNT_4_BIT});
	TextureHandler th;
	Scene s((float)w.getSCExtent().width / (float)w.getSCExtent().height);
	PPStep volumetrics(&w, "vol");

	Mesh m("../../resources/models/objs/cube.obj");
	POMPCData pompcdat;
	RenderPassInfo* sm_rp = createSMRenderPass(s, w);

	RenderPassInfo* main_rp = createMainRP(s, w, volumetrics);
	size_t default_pidx = createDefaultPipeline(s, w, main_rp);
	size_t instanced_pidx = createInstancedPipeline(s, w, main_rp);
	// size_t textured_pidx = createTexturedPipeline(s, w, main_rp);
	size_t pom_pidx = createPOMPipeline(s, w, main_rp);
	main_rp->setScenePC(pom_pidx, &pompcdat);
	size_t shadowcatch_pidx = createShadowReceivePipeline(s, w, main_rp);
	DNSScenePCData sc_scene_pcd;
	main_rp->setScenePC(shadowcatch_pidx, &sc_scene_pcd);
	main_rp->addMesh(&m, VK_NULL_HANDLE, &m.getModelMatrix(), default_pidx);
	RenderPassInfo uirpi = createUIRPI(w);

	/*
	 * UI Setup
	 */
	/*
	 * As of yet unresolved issue: UI is strangely offset when rendered on Linux Mint machine w/ Nvidia graphics card, 1080p monitor
	 */

	std::wstring log = L"";
	const size_t logmaxlines = 20;
	uint64_t lastfpstime = SDL_GetTicks();
	const uint64_t maxfpstime = 1000;
	float fpstot = 0, framevar, frameavg;
	std::vector<float> frametimes;
	size_t numf = 0;
	UIHandler ui(uirpi.getRenderSet(0).pipeline, w.getSCExtent());
	UIContainer* leftsidebar = ui.addComponent(UIContainer());
	// leftsidebar->setPos(UICoord(0, 0));
	// leftsidebar->setExt(UICoord(1000, w.getSCExtent().height));
	leftsidebar->setPos(UICoord(0, 2 * w.getSCExtent().height));
	leftsidebar->setExt(UICoord(1000, -2 * w.getSCExtent().height));
	// leftsidebar->setBGCol({0.1, 0.1, 0.1, 0.9});
	leftsidebar->setBGCol({0, 0.8, 0, 1});
	UIText* logtext = leftsidebar->addChild(UIText());
	logtext->setPos(UICoord(0, 0));
	logtext->setBGCol({0, 0, 0, 0});
	UIText* camtext = ui.addComponent(UIText());
	camtext->setPos(UICoord(1000, w.getSCExtent().height));
	UIText* fpstext = ui.addComponent(UIText());
	fpstext->setPos(UICoord(w.getSCExtent().width - 300, 0));
	// s.getRenderPass(1).setUI(&ui, 0);

	/*
	 * Lighting
	 */

	DirectionalLightInitInfo l_ii;
	l_ii.super_light.c = glm::vec3(1, 1, 0);
	l_ii.super_directional.f = glm::vec3(1, -1, 0);
	DirectionalLight* sl = s.addDirectionalLight(DirectionalLight(l_ii), {{1024, 1024}});
	l_ii.super_light.c = glm::vec3(0, 0, 1);
	l_ii.super_directional.f = glm::vec3(0, -1, 1);
	DirectionalLight* noshad = s.addDirectionalLight(DirectionalLight(l_ii), {});

	// new
	PipelineInfo sm_pipeline = createSMPipelineTemplate(sm_rp); // added several times to different render sets w/ different dyn viewport/scissor
	std::vector<size_t> sm_p_idxs = s.addSMPipeline(*sl, sm_pipeline, *sm_rp, nullptr);
	std::cout << sm_p_idxs.size() << "\n";
	for (size_t i = 0; i < sm_p_idxs.size(); i++) sm_rp->setScenePC(sm_p_idxs[i], &sl->getSMDatum(i).getVP()); 

	/*
	 * Misc Mesh Instantiation
	 */
	std::vector<InstancedMeshData> imdatatemp;
	InstancedMesh im = createCubeRing(imdatatemp, 32, 3);
	VkDescriptorSet temp;
	GH::createDS(main_rp->getRenderSet(instanced_pidx).pipeline, temp);
	GH::updateDS(temp, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {}, im.getInstanceUB().getDBI());
	main_rp->addMesh(&im, temp, nullptr, instanced_pidx);

	Mesh plane("../../resources/models/objs/plane.obj");
	GH::createDS(main_rp->getRenderSet(shadowcatch_pidx).pipeline, temp);
	DNSObjectPCData plane_pcd = {s.addLightCatcher(&plane, temp, {0, 1}, {}, {}), plane.getModelMatrix()};
	main_rp->addMesh(&plane, temp, &plane_pcd, shadowcatch_pidx); // plane receives shadows

	/*
	Mesh plane2("../../resources/models/objs/plane.obj", VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL | VERTEX_BUFFER_TRAIT_TANGENT | VERTEX_BUFFER_TRAIT_BITANGENT);
	plane2.setPos(glm::vec3(20, 0, 0));
	main_rp->addMesh(&plane2, VK_NULL_HANDLE, &plane2.getModelMatrix(), 3);
	*/

	std::vector<LODFuncData> tempfd;
	LODMesh suz = createLODSuzanne(s, tempfd);
	/*
	TextureSet t("../resources/textures/uvgrid");
	t.setDiffuseSampler(th.addSampler("bilinear", VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE));
	GH::createDS(s.getRenderPass(1).getRenderSet(2).pipeline, temp);
	GH::updateDS(temp, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, t.getDiffuse().getDII(), {});
	*/
	main_rp->addMesh(&suz, VK_NULL_HANDLE, &suz.getModelMatrix(), default_pidx);
	s.addShadowCaster(&suz, {0});
	m.setPos(glm::vec3(-5, 10, -5));
	s.addShadowCaster(&m, {0});

	Mesh tree("resources/models/tree.obj");
	main_rp->addMesh(&tree, VK_NULL_HANDLE, &tree.getModelMatrix(), default_pidx);
	tree.setPos(glm::vec3(-10, 0, 0));
	tree.setScale(glm::vec3(4, 2, 4));
	s.addShadowCaster(&tree, {0});

	for (size_t i = 0; i < sm_p_idxs.size(); i++) {
		sm_rp->addMesh(&tree, VK_NULL_HANDLE, &tree.getModelMatrix(), sm_p_idxs[i]); 
		sm_rp->addMesh(&suz, VK_NULL_HANDLE, &suz.getModelMatrix(), sm_p_idxs[i]); 
	}

	GH::updateDS(volumetrics.getDS(), 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sl->getSMDatum(0).getSM()->getDII(), {}); 

	w.addTasks(s.getDrawTasks());

	w.addTask(cbRecTaskTemplate(cbRecTaskRenderPassTemplate(VK_NULL_HANDLE, nullptr, 0, {0, 0}, 0, nullptr)));
	w.addTask(cbRecTaskTemplate([scis = w.getSCImages(), dst = volumetrics.getSrc()] (uint8_t scii, VkCommandBuffer& c) {PPStep::recordCopy(scii, c, scis, dst);}));
	w.addTask(cbRecTaskTemplate(volumetrics.getRTRPT()));
	w.addTask(cbRecTaskTemplate([rs = volumetrics.getRS()] (uint8_t scii, VkCommandBuffer& c) {PPStep::recordDraw(scii, c, rs);}));

	w.addTask(cbRecTaskTemplate(uirpi.getRPT()));
	w.addTask(cbRecTaskTemplate([&ui, rp = uirpi.getRenderPass(), fb = uirpi.getFramebuffers()]
		(uint8_t scii, VkCommandBuffer& c) {
		ui.recordDraw(fb[scii], rp, c); // might need to mod fb idx against ui's own scii count
	}));

	/*
	 * Physics Scene Setup
	 */

	PhysicsHandler ph;

	PointCollider* pc = static_cast<PointCollider*>(ph.addCollider(PointCollider()));
	pc->setPos(glm::vec3(-5, 10, -5));
	pc->applyForce(glm::vec3(0, -9.807, 0));

	PlaneCollider* plc = static_cast<PlaneCollider*>(ph.addCollider(PlaneCollider(glm::vec3(0, 1, 0))));
	plc->setMass(std::numeric_limits<float>::infinity());

	ColliderPair* colpair = ph.addColliderPair(ColliderPair(pc, plc), true);
	colpair->setOnCollide({[] (void* d) {
			std::wstring* l = static_cast<std::wstring*>(d);
			l->insert(0, L"Point collided with plane!\n");
		}, &log});

	/*
	 * Input Scripting
	 */

	InputHandler ih;
	glm::vec3 movementdir;
	ih.addHold(InputHold(SDL_SCANCODE_W, [&movementdir, c = s.getCamera()] () { movementdir += glm::vec3(0, 1, 0); }));
	ih.addHold(InputHold(SDL_SCANCODE_A, [&movementdir, c = s.getCamera()] () { movementdir -= glm::cross(c->getForward(), glm::vec3(0, 1, 0)); }));
	ih.addHold(InputHold(SDL_SCANCODE_S, [&movementdir, c = s.getCamera()] () { movementdir -= glm::vec3(0, 1, 0); }));
	ih.addHold(InputHold(SDL_SCANCODE_D, [&movementdir, c = s.getCamera()] () { movementdir += glm::cross(c->getForward(), glm::vec3(0, 1, 0));; }));
	ih.addHold(InputHold(SDL_SCANCODE_E, [&movementdir, c = s.getCamera()] () { movementdir += glm::normalize(c->getForward()); }));
	ih.addHold(InputHold(SDL_SCANCODE_Q, [&movementdir, c = s.getCamera()] () { movementdir -= glm::normalize(c->getForward()); }));
	// TODO: consider using a sigmoid to modulate FOVY input
	ih.addHold(InputHold(SDL_SCANCODE_UP, [&movementdir, c = s.getCamera()] () { if (c->getFOVY() > FOV_SENS) c->setFOVY(c->getFOVY() - FOV_SENS); }));
	ih.addHold(InputHold(SDL_SCANCODE_DOWN, [&movementdir, c = s.getCamera()] () { if (c->getFOVY() < glm::pi<float>() - FOV_SENS) c->setFOVY(c->getFOVY() + FOV_SENS); }));
	ih.addCheck(InputCheck(SDL_EVENT_KEY_DOWN, [&log, &w, &s, &suz, &plane, pc, sm_pipeline, sm_rp, shadowcatch_pidx] (const SDL_Event& e) { 
		if (e.key.scancode == SDL_SCANCODE_H) {
			log.insert(0, L"Hello World! @ " + getTimestamp() + L"\n");
			return true;
		}
		if (e.key.scancode == SDL_SCANCODE_L) {
			if (s.getNumDirLights() < SCENE_MAX_DIR_LIGHTS && s.getNumSCLights() < SCENE_MAX_SC_LIGHTS) {
				addLight(w, s, suz, plane, sm_pipeline, sm_rp, shadowcatch_pidx);
				log.insert(0, L"Added Light looking toward ["
						 + std::to_wstring(s.getDirLights()[s.getNumDirLights() - 1].getForward().x) + L", " 
						 + std::to_wstring(s.getDirLights()[s.getNumDirLights() - 1].getForward().y) + L", " 
						 + std::to_wstring(s.getDirLights()[s.getNumDirLights() - 1].getForward().z) + L"] w/ col ["
						 + std::to_wstring(s.getDirLights()[s.getNumDirLights() - 1].getCol().x) + L", " 
						 + std::to_wstring(s.getDirLights()[s.getNumDirLights() - 1].getCol().y) + L", " 
						 + std::to_wstring(s.getDirLights()[s.getNumDirLights() - 1].getCol().z) + L"] \n");
			}
			else log.insert(0, L"Too many lights to add another!\n");
			return true;
		}
		if (e.key.scancode == SDL_SCANCODE_C) {
			pc->setPos(glm::vec3(-5, 10, -5));
		}
		return false;
	}));


	ph.start();
	float theta = 0;
	while (w.frameCallback()) {
		/*
		 * Input Update
		 */
		movementdir = glm::vec3(0);
		ih.update();
		SDL_PumpEvents();
		if (movementdir != glm::vec3(0)) {
			s.getCamera()->setPos(s.getCamera()->getPos() + MOVEMENT_SENS * glm::normalize(movementdir));
			s.getCamera()->setForward(-s.getCamera()->getPos());
			

			pompcdat = (POMPCData) {s.getCamera()->getVP(), glm::vec4(s.getCamera()->getPos().x, s.getCamera()->getPos().y, s.getCamera()->getPos().z, 1)};
		}
		s.getCamera()->updateView();
		s.getCamera()->updateProj();
		sc_scene_pcd.vp = s.getCamera()->getVP();
		sc_scene_pcd.c_pos = s.getCamera()->getPos();
		s.updateSMDCascade(*sl, 0, glm::vec2(0, 1));
		// can't just do in loop cuz need time;
		volumetrics.updatePC((temp_pc_dat){s.getCamera()->getVP(), sl->getSMDatum(0).getVP(), glm::inverse(s.getCamera()->getVP()), s.getCamera()->getPos(), (float)SDL_GetTicks() / 1000.f});

		/*
		 * UI Update
		 */
		size_t numlines = 0;
		for (wchar_t c : log) if (c == L'\n') numlines++;
		if (numlines > logmaxlines) log = log.substr(0, log.find_last_of(L'\n'));
		if (logtext->getText() != log) logtext->setText(log);
		std::wstring camtextstring = L"[" + std::to_wstring(s.getCamera()->getPos().x) + L", " + std::to_wstring(s.getCamera()->getPos().y) + L", " + std::to_wstring(s.getCamera()->getPos().z) + L"], FOV = " 
			 + std::to_wstring(s.getCamera()->getFOVY()) + L"º";
		if (camtext->getText() != camtextstring) camtext->setText(camtextstring);
		frametimes.push_back(1.f / ph.getDT());
		fpstot += frametimes.back();
		numf++;
		if (SDL_GetTicks() - lastfpstime > maxfpstime) {
			framevar = 0;
			frameavg = fpstot / (float)numf;
			// frametimes name is a bit misleading...
			for (float t : frametimes) framevar += pow(t - frameavg, 2);
			framevar /= (float)(numf - 1);
			fpstext->setText(std::to_wstring(frameavg) + L" fps"
					 + L"\nn = " + std::to_wstring(numf)
					 + L"\nvar = " + std::to_wstring(framevar));
			fpstot = 0;
			numf = 0;
			lastfpstime = SDL_GetTicks();
			frametimes.clear();
		}

		throbCubeRing(im, imdatatemp, 0.5, (float)SDL_GetTicks() / 1000);	

		m.setPos(pc->getPos() + glm::vec3(0, 1, 0));
		// plane.setPos(plc->getPos());

		ph.update();
	}
	vkQueueWaitIdle(GH::getGenericQueue());
	uirpi.destroy();

	return 0;
}

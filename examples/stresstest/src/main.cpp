#include "Scene.h"
#include "TextureHandler.h"
#include "PhysicsHandler.h"
#include "InputHandler.h"

#define ST_VERBOSE_STORAGE
#define ST_MAX_SC_LIGHTS 8u
#define ST_MAX_DIR_LIGHTS 8u
#define CAMERA_SENS 0.01f
#define MOVEMENT_SENS 50.f
#define MOVEMENT_CAP 2.f

#define VB_TRAIT_ALL VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL | VERTEX_BUFFER_TRAIT_TANGENT | VERTEX_BUFFER_TRAIT_BITANGENT

typedef struct DNSScenePCData {
	glm::mat4 vp;
	glm::vec3 c_pos;
	float padding;
} DNSScenePCData;

RenderPassInfo* createMainRenderPass(Scene& s, WindowInfo& w) {
	VkRenderPass r;
	VkAttachmentDescription a_d[3] {{
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
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	}};
	VkAttachmentReference a_r[3] {
		{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
		{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
		{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
	};
	GH::createRenderPass(r, 3, &a_d[0], &a_r[0], &a_r[2], &a_r[1]);
	std::vector<const ImageInfo*> a_imgs = {&w.getMSAAImage(), w.getDepthBuffer(), w.getSCImages()};
	RenderPassInfo rpi(r, w.getNumSCIs(), w.getMSAAImage().extent, {{0.3, 0.3, 0.3, 1}, {1, 0}}, a_imgs, 2);

	return s.addRenderPass(rpi);
}

size_t createDNSPipeline(RenderPassInfo& rpi, Scene& s, const WindowInfo& w) {
	PipelineInfo p;
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "dns";
	VkDescriptorSetLayoutBinding dtbindings[6] {{
			0,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			1,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		}, {
			1,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			ST_MAX_SC_LIGHTS,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		}, {
			2,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			1,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		}, {
			3,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		}, {
			4,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		}, {
			5,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
	}};
	p.descsetlayoutci = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		6, &dtbindings[0]
	};
	p.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DNSScenePCData)};
	p.objpushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(DNSScenePCData), sizeof(glm::mat4)};
	p.vertexinputstateci = Mesh::getVISCI(
			VERTEX_BUFFER_TRAIT_POSITION
			| VERTEX_BUFFER_TRAIT_UV 
			| VERTEX_BUFFER_TRAIT_NORMAL
			| VERTEX_BUFFER_TRAIT_TANGENT
			| VERTEX_BUFFER_TRAIT_BITANGENT);
	p.depthtest = true;
	p.extent = w.getSCExtent();
	p.renderpass = rpi.getRenderPass();
	p.msaasamples = w.getMSAASamples();
	GH::createPipeline(p);
	Mesh::ungetVISCI(p.vertexinputstateci);
	return rpi.addPipeline(p, nullptr);
}

size_t createDNSInstancedPipeline(RenderPassInfo& rpi, Scene& s, const WindowInfo& w) {
	PipelineInfo p;
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "dnsinst";
	VkDescriptorSetLayoutBinding dtbindings[7] {{
			0,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			1,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		}, {
			1,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			ST_MAX_SC_LIGHTS,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		}, {
			2,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			1,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		}, {
			3,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		}, {
			4,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		}, {
			5,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
	}, {
			6,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			1,
			VK_SHADER_STAGE_VERTEX_BIT,
			nullptr
	}};
	p.descsetlayoutci = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		7, &dtbindings[0]
	};
	p.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DNSScenePCData)};
	p.vertexinputstateci = Mesh::getVISCI(
			VERTEX_BUFFER_TRAIT_POSITION
			| VERTEX_BUFFER_TRAIT_UV 
			| VERTEX_BUFFER_TRAIT_NORMAL
			| VERTEX_BUFFER_TRAIT_TANGENT
			| VERTEX_BUFFER_TRAIT_BITANGENT);
	p.depthtest = true;
	p.extent = w.getSCExtent();
	p.renderpass = rpi.getRenderPass();
	p.msaasamples = w.getMSAASamples();
	GH::createPipeline(p);
	Mesh::ungetVISCI(p.vertexinputstateci);
	return rpi.addPipeline(p, nullptr);
}

int main() {
	/*
	 * 
	 */
	GHInitInfo ghii;
	// shouldn't be required in vulkan 1.2+, but MoltenVK sucks
	ghii.dexts.push_back("VK_KHR_depth_stencil_resolve");
	ghii.dexts.push_back("VK_KHR_create_renderpass2");
	ghii.dexts.push_back("VK_KHR_multiview");
	ghii.dexts.push_back("VK_KHR_maintenance2");
	GH g(ghii);
	g.setShaderDirectory("../../resources/shaders/SPIRV/");
	WindowInitInfo wii;
	wii.msaa = VK_SAMPLE_COUNT_4_BIT;
	WindowInfo w(wii);
	Scene s(w);

	/*
	 * Renderpasses & Pipelines
	 */
	RenderPassInfo* main_rp = createMainRenderPass(s, w);
	size_t dnsp_idx = createDNSPipeline(*main_rp, s, w);
	size_t dnsinstp_idx = createDNSInstancedPipeline(*main_rp, s, w);

	/*
	 * This test scene is meant to be strenuous but not unfairly so.
	 * E.g., there are many meshes, but they will be instanced if feasible,
	 * textures are high-resolution, but not higher than needed
	 *
	 * Several trees are used to boost poly count and make complex shadows. TODO
	 * A player mesh is bound to the first-person camera to track near-field shadows. TODO
	 * Post-processing effects are added (although most of these should not scale with scene complexity, their algorithms may fail on a complex scene) TODO
	 */

	/*
	 * Lighting Setup
	 */
	DirectionalLight* sc_dl = s.addDirectionalLight(DirectionalLight(
		{{glm::vec3(0, 20, 0), glm::vec3(1), {1024, 1024}}, 
		DIRECTIONAL_LIGHT_TYPE_ORTHO, glm::vec3(0, -1, 0)}));

	/*
	 * Textures
	 */
	TextureHandler th;
	th.addSampler("near", VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_FALSE);
	th.addSampler("linearmag", VK_FILTER_LINEAR, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_FALSE);
	th.setDefaultSampler(th.getSampler("near"));
	th.addSet("soil", TextureSet("resources/textures/soil", th.getSampler("linearmag")));

	/*
	 * Meshes
	 */
	Mesh ground("resources/models/ground.obj", VB_TRAIT_ALL);
	VkDescriptorSet ds_temp;
	const TextureSet& set = th.getSet("soil");
	GH::createDS(main_rp->getRenderSet(dnsp_idx).pipeline, ds_temp);
	GH::updateDS(ds_temp, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, set.getTexture("diffuse").getDII(), {});
	GH::updateDS(ds_temp, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, set.getTexture("normal").getDII(), {});
	GH::updateDS(ds_temp, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, set.getTexture("specular").getDII(), {});
	s.hookupLightCatcher(&ground, ds_temp, {}, {0});
	main_rp->addMesh(&ground, ds_temp, &ground.getModelMatrix(), dnsp_idx);
#ifdef ST_VERBOSE_STORAGE
	GH_LOG_RESOURCE_SIZE(Ground VB, ground.getVertexBuffer().size);
	GH_LOG_RESOURCE_SIZE(Ground IB, ground.getIndexBuffer().size);
	GH_LOG_RESOURCE_SIZE(Ground Diffuse, set.getTexture("diffuse").extent.width * set.getTexture("diffuse").extent.height * set.getTexture("diffuse").getPixelSize());
	GH_LOG_RESOURCE_SIZE(Ground Normal, set.getTexture("normal").extent.width * set.getTexture("normal").extent.height * set.getTexture("normal").getPixelSize());
	GH_LOG_RESOURCE_SIZE(Ground Specular, set.getTexture("specular").extent.width * set.getTexture("specular").extent.height * set.getTexture("specular").getPixelSize());
#endif

	const uint8_t trees_n = 50;
	const float trees_offset = 20, trees_range = 20;
	std::vector<InstancedMeshData> trees_imdata(trees_n);
	srand(0);
	for (uint8_t i = 0; i < trees_n; i++) {
		trees_imdata[i].m = glm::mat4(1);
		trees_imdata[i].m = glm::rotate(trees_imdata[i].m, glm::two_pi<float>()*(float)rand()/(float)RAND_MAX, glm::vec3(0, 1, 0));
		trees_imdata[i].m = glm::translate(trees_imdata[i].m, glm::vec3(trees_offset, 0, 0) + trees_range * glm::vec3((float)rand() / (float)RAND_MAX, 0, 0));
	}
	InstancedMesh tree_body("resources/models/tree_body.obj", trees_imdata, VB_TRAIT_ALL);
	GH::createDS(main_rp->getRenderSet(dnsinstp_idx).pipeline, ds_temp);
	GH::updateDS(ds_temp, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, set.getTexture("diffuse").getDII(), {});
	GH::updateDS(ds_temp, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, set.getTexture("normal").getDII(), {});
	GH::updateDS(ds_temp, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, set.getTexture("specular").getDII(), {});
	GH::updateDS(ds_temp, 6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {}, tree_body.getInstanceUB().getDBI());
	s.hookupLightCatcher(&tree_body, ds_temp, {}, {0});
	main_rp->addMesh(&tree_body, ds_temp, nullptr, dnsinstp_idx);
#ifdef ST_VERBOSE_STORAGE
	GH_LOG_RESOURCE_SIZE(Tree Body VB, tree_body.getVertexBuffer().size);
	GH_LOG_RESOURCE_SIZE(Tree Body IB, tree_body.getIndexBuffer().size);
	GH_LOG_RESOURCE_SIZE(Tree Body UB, tree_body.getInstanceUB().size);
	GH_LOG_RESOURCE_SIZE(Tree Body Diffuse, set.getTexture("diffuse").extent.width * set.getTexture("diffuse").extent.height * set.getTexture("diffuse").getPixelSize());
	GH_LOG_RESOURCE_SIZE(Tree Body Normal, set.getTexture("normal").extent.width * set.getTexture("normal").extent.height * set.getTexture("normal").getPixelSize());
	GH_LOG_RESOURCE_SIZE(Tree Body Specular, set.getTexture("specular").extent.width * set.getTexture("specular").extent.height * set.getTexture("specular").getPixelSize());
#endif
	InstancedMesh tree_leaves("resources/models/tree_leaves.obj", trees_imdata, VB_TRAIT_ALL);
	s.hookupLightCatcher(&tree_leaves, ds_temp, {}, {0});
	main_rp->addMesh(&tree_leaves, ds_temp, nullptr, dnsinstp_idx);



	s.getCamera()->setPos(glm::vec3(20, 20, 0));
	s.getCamera()->setForward(glm::vec3(-1, -1, 0));
	DNSScenePCData dns_s_pc {s.getCamera()->getVP(), s.getCamera()->getPos()};
	main_rp->setScenePC(dnsp_idx, &dns_s_pc);
	main_rp->setScenePC(dnsinstp_idx, &dns_s_pc);

	w.addTasks(s.getDrawTasks());
	
	/*
	 * Physics
	 */
	PhysicsHandler ph;
	SphereCollider* player_c = (SphereCollider*)ph.addCollider(SphereCollider(2.f));
	player_c->setPos(glm::vec3(0, 5, 0));
	player_c->applyForce(player_c->getMass() * glm::vec3(0, -9.81, 0));

	PlaneCollider* ground_c = (PlaneCollider*)ph.addCollider(PlaneCollider(glm::vec3(0, 1, 0)));
	ground_c->setMass(std::numeric_limits<float>::infinity());

	ColliderPair* player_ground_cp = ph.addColliderPair(ColliderPair(player_c, ground_c), true);

	/*
	 * Input
	 */
	InputHandler ih;
	glm::vec3 movementdir;
	bool no_jump;
	ih.addHold(InputHold(SDL_SCANCODE_W, [&movementdir, c = s.getCamera()] () { movementdir += glm::normalize(c->getForward() * glm::vec3(1, 0, 1)); }));
	ih.addHold(InputHold(SDL_SCANCODE_A, [&movementdir, c = s.getCamera()] () { movementdir -= c->getRight(); }));
	ih.addHold(InputHold(SDL_SCANCODE_S, [&movementdir, c = s.getCamera()] () { movementdir -= glm::normalize(c->getForward() * glm::vec3(1, 0, 1)); }));
	ih.addHold(InputHold(SDL_SCANCODE_D, [&movementdir, c = s.getCamera()] () { movementdir += c->getRight(); }));

	ih.addCheck(InputCheck(SDL_EVENT_KEY_DOWN, [&ph, player_c, &no_jump] (const SDL_Event& e) {
		if (e.key.scancode == SDL_SCANCODE_SPACE && !e.key.repeat && !no_jump) {
			ph.addTimedForce({player_c, player_c->getMass() * glm::vec3(0, 20, 0), 0.2});
			return true;
		}
		return false;
	}));
	player_ground_cp->setOnDecouple({[] (void* d) { *(bool*)d = true; }, &no_jump});
	player_ground_cp->setOnCouple({[] (void* d) { *(bool*)d = false; }, &no_jump});

	ih.addCheck(InputCheck(SDL_EVENT_MOUSE_MOTION, [c = s.getCamera()] (const SDL_Event& e) {
		c->setForward(c->getForward() + CAMERA_SENS * (c->getRight() * e.motion.xrel + c->getUp() * -e.motion.yrel));
		return true;
	}));

	s.getCamera()->setPos(player_c->getPos());
	dns_s_pc = (DNSScenePCData){s.getCamera()->getVP(), s.getCamera()->getPos()};

	ph.start();
	while (w.frameCallback()) {
		movementdir = glm::vec3(0);
		ih.update();
		SDL_PumpEvents();
		if (movementdir != glm::vec3(0) && glm::length(player_c->getVel()) < MOVEMENT_CAP) {
			movementdir = glm::normalize(movementdir);
			ph.addTimedForce({player_c, MOVEMENT_SENS * movementdir, 0});
		}

		ph.update();

		s.getCamera()->setPos(player_c->getPos());
		dns_s_pc = (DNSScenePCData){s.getCamera()->getVP(), s.getCamera()->getPos()};
	}

	vkQueueWaitIdle(GH::getGenericQueue());

	return 0;
}

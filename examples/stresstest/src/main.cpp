#define ST_TS_WIN
#ifdef ST_TS_WIN
#include "UIHandler.h"
#endif
#include "Scene.h"
#include "TextureHandler.h"
#include "PhysicsHandler.h"
#include "InputHandler.h"

#define ST_VERBOSE_STORAGE
#define ST_MAX_SC_LIGHTS 8u
#define ST_MAX_DIR_LIGHTS 8u
#define CAMERA_SENS 0.01f
#define MOVEMENT_SENS 50.f
#define MOVEMENT_CAP 20.f

#define VB_TRAIT_ALL (VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL | VERTEX_BUFFER_TRAIT_TANGENT | VERTEX_BUFFER_TRAIT_BITANGENT)

typedef struct [[gnu::packed]] DNSScenePCData {
	glm::mat4 vp;
	glm::vec3 c_pos;
} DNSScenePCData;

typedef struct [[gnu::packed]] DNSObjectPCData {
	uint32_t catcher_idx;
	glm::mat4 m;
} DNSObjectPCData;

typedef struct SMPCData {
	glm::mat4 vp;
} SMPCData;

typedef struct InputData {
	glm::vec3 movementdir = glm::vec3(0);
} InputData;

#ifdef ST_TS_WIN
#define STATS_N 64
#define ST_STATS_N_BINS 5
#define ST_STATS_BIN_MIN 55.f
#define ST_STATS_BIN_MAX 65.f
typedef struct StatsData {
	size_t i;
	uint64_t last_TOL, last_frame_done,    // timestamps
		TOLs[STATS_N], frame_dones[STATS_N]; // time deltas
	UIContainer* hist_bars[ST_STATS_N_BINS];
	UIText* summary_text;
} StatsData;

void initStatsData(StatsData& sd) {
	sd.last_TOL = SDL_GetTicks();
	sd.last_frame_done = sd.last_TOL;
	sd.i = 0;
}

void updateStatsData(StatsData& sd) {
	sd.i++;
	if (sd.i == STATS_N) {
		sd.i = 0;
		float tot_fps[STATS_N], mean_TOL = 0, mean_frame_done = 0, mean_total = 0;
		for (size_t i = 0; i < STATS_N; i++) {
			tot_fps[i] = 1000.f/(float)(sd.TOLs[i] + sd.frame_dones[i]);
			mean_TOL += (float)sd.TOLs[i] / 1000.f;
			mean_frame_done += (float)sd.frame_dones[i] / 1000.f;
			mean_total += (float)(sd.frame_dones[i] + sd.TOLs[i]) / 1000.f;
		}
		mean_TOL /= STATS_N;
		mean_frame_done /= STATS_N;
		mean_total /= STATS_N;
		float mean = 0, var = 0;
		for (size_t i = 0; i < STATS_N; i++) mean += tot_fps[i];
		mean /= (float)STATS_N;
		for (size_t i = 0; i < STATS_N; i++) var += pow(tot_fps[i] - mean, 2);
		var /= (float)STATS_N - 1.f;
		float beta = sqrt(var * 6 / pow(3.14, 2));
		float mu = mean - beta * 0.5772;

		size_t bin_counts[ST_STATS_N_BINS];
		for (size_t j = 0; j < ST_STATS_N_BINS; j++) bin_counts[j] = 0;
		for (size_t i = 0; i < STATS_N; i++) {
			float bin_width = (ST_STATS_BIN_MAX - ST_STATS_BIN_MIN) / (float)ST_STATS_N_BINS, 
						bin_min = ST_STATS_BIN_MIN, 
						bin_max = bin_min + bin_width;
			if (tot_fps[i] > ST_STATS_BIN_MIN && tot_fps[i] < ST_STATS_BIN_MAX)
				bin_counts[(size_t)floor((tot_fps[i] - ST_STATS_BIN_MIN) / bin_width)]++;
		}
		for (size_t j = 0; j < ST_STATS_N_BINS; j++) sd.hist_bars[j]->setExt({sd.hist_bars[j]->getExt().x, -10 * (float)bin_counts[j]});

		float fps_norm_pivot = sqrt(STATS_N/var)*(50.f-mean);
		sd.summary_text->setText(
			L"Avg FPS: " + std::to_wstring(mean).substr(0, 4)
			+ L"\nSD FPS: " + std::to_wstring(sqrt(var)).substr(0, 4)
			+ L"\nP(FPS < 50): " + std::to_wstring(0.5*erfc(-fps_norm_pivot/sqrt(2))).substr(0, 6)
			+ L"\n" + std::to_wstring(mean_TOL/mean_total*100.f).substr(0, 5) + L"\% misc\n"
			+ std::to_wstring(mean_frame_done/mean_total*100.f).substr(0, 5) + L"\% graphics");
	}
}
#endif

glm::vec3 apply(glm::mat4 m, glm::vec3 v) {
	glm::vec4 u = m * glm::vec4(v.x, v.y, v.z, 1);
	return glm::vec3(u.x, u.y, u.z) / u.w;
}

RenderPassInfo* createSMRenderPass(Scene& s, WindowInfo& w) {
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
			1,
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
	p.objpushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(DNSScenePCData), sizeof(DNSObjectPCData)};
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
			1,
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
	p.objpushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(DNSScenePCData), sizeof(uint32_t)};
	p.vertexinputstateci = Mesh::getVISCI(VB_TRAIT_ALL);
	p.depthtest = true;
	p.extent = w.getSCExtent();
	p.renderpass = rpi.getRenderPass();
	p.msaasamples = w.getMSAASamples();
	GH::createPipeline(p);
	Mesh::ungetVISCI(p.vertexinputstateci);
	return rpi.addPipeline(p, nullptr);
}

PipelineInfo createSMPipeline(RenderPassInfo& rpi) {
	PipelineInfo p;
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "shadowmap";
	p.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)};
	p.objpushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), sizeof(glm::mat4)};
	p.vertexinputstateci = Mesh::getVISCI(VB_TRAIT_ALL, VB_TRAIT_ALL ^ VERTEX_BUFFER_TRAIT_POSITION);
	p.depthtest = true;
	p.cullmode = VK_CULL_MODE_FRONT_BIT;
	p.renderpass = rpi.getRenderPass();
	p.dyn_viewport = true;
	GH::createPipeline(p);
	Mesh::ungetVISCI(p.vertexinputstateci);
	return p;
}

PipelineInfo createSMInstancedPipeline(RenderPassInfo& rpi) {
	PipelineInfo p;
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "sminst";
	VkDescriptorSetLayoutBinding dtbindings[1] {{
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
		1, &dtbindings[0]
	};
	p.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(SMPCData)};
	p.vertexinputstateci = Mesh::getVISCI(VB_TRAIT_ALL, VB_TRAIT_ALL ^ VERTEX_BUFFER_TRAIT_POSITION);
	p.depthtest = true;
	p.cullmode = VK_CULL_MODE_FRONT_BIT;
	p.renderpass = rpi.getRenderPass();
	p.dyn_viewport = true;
	GH::createPipeline(p);
	Mesh::ungetVISCI(p.vertexinputstateci);
	return p;
}

void initInput(InputHandler& ih, Scene& s, WindowInfo& w, InputData& id) {
	ih.addHold(InputHold(SDL_SCANCODE_W, [&id, c = s.getCamera()] () { id.movementdir += glm::normalize(c->getForward() * glm::vec3(1, 0, 1)); }));
	ih.addHold(InputHold(SDL_SCANCODE_A, [&id, c = s.getCamera()] () { id.movementdir -= glm::cross(c->getForward(), glm::vec3(0, 1, 0)); }));
	ih.addHold(InputHold(SDL_SCANCODE_S, [&id, c = s.getCamera()] () { id.movementdir -= glm::normalize(c->getForward() * glm::vec3(1, 0, 1)); }));
	ih.addHold(InputHold(SDL_SCANCODE_D, [&id, c = s.getCamera()] () { id.movementdir += glm::cross(c->getForward(), glm::vec3(0, 1, 0)); }));

	ih.addCheck(InputCheck(SDL_EVENT_MOUSE_MOTION, [c = s.getCamera()] (const SDL_Event& e) {
		c->setForward(c->getForward() + CAMERA_SENS * (glm::cross(c->getForward(), glm::vec3(0, 1, 0)) * e.motion.xrel + glm::vec3(0, 1, 0) * -e.motion.yrel));
		return true;
	}));
	ih.addCheck(InputCheck(SDL_EVENT_MOUSE_BUTTON_DOWN, [&w] (const SDL_Event& e) {
		if (e.button.windowID == SDL_GetWindowID(w.getSDLWindow())) {
			SDL_SetWindowRelativeMouseMode(w.getSDLWindow(), true);
			return true;
		}
		return false;
	}));
	ih.addCheck(InputCheck(SDL_EVENT_MOUSE_BUTTON_UP, [&w] (const SDL_Event& e) {
		SDL_SetWindowRelativeMouseMode(w.getSDLWindow(), false);
		return true;
	}));
}

void updateCascade(Scene& s, Light& l, size_t smd_idx) {
	LightSMData& smd = l.getSMDatum(smd_idx);
	smd.clearFocus();
	glm::mat4 vp_inv = glm::inverse(s.getCamera()->getVP());
	glm::vec2 z_range;
	glm::vec4 temp = s.getCamera()->getProj() * glm::vec4(0, 0, -((float)smd_idx / 3) * s.getCamera()->getFarClip() - s.getCamera()->getNearClip(), 1);
	z_range.x = temp.z / temp.w;
	temp = s.getCamera()->getProj() * glm::vec4(0, 0, -(((float)smd_idx + 1) / 3) * s.getCamera()->getFarClip(), 1);
	z_range.y = temp.z / temp.w;
	std::cout << z_range.x << ", " << z_range.y << '\n';
	/*
	if (smd_idx == 0) z_range = glm::vec2(0.1, 0.8);
	else if (smd_idx == 1) z_range = glm::vec2(0.8, 0.9);
	else z_range = glm::vec2(0.9, 1);
	*/
	for (float x = -1; x < 2; x += 2)
	for (float y = -1; y < 2; y += 2)
	for (float z = z_range.x; z <= z_range.y; z += z_range.y - z_range.x) {
		smd.addVecToFocus(ProjectionBase::applyHomo(vp_inv, glm::vec3(x, y, z)));
	}
	s.updateSMD(l, smd_idx);
}

#ifdef ST_TS_WIN
RenderPassInfo* createTSRenderPass(Scene& s, WindowInfo& w) {
	VkRenderPass r;
	VkAttachmentDescription a_d[2] {{
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
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	}};
	VkAttachmentReference a_r[2] {
		{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
		{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
	};
	GH::createRenderPass(r, 2, &a_d[0], &a_r[0], nullptr, &a_r[1]);
	std::vector<const ImageInfo*> a_imgs = {w.getSCImages(), w.getDepthBuffer()};
	RenderPassInfo rpi(r, w.getNumSCIs(), w.getSCExtent(), {{0.3, 0.3, 0.3, 1}, {1, 0}}, a_imgs, 0);

	return s.addRenderPass(rpi);
}

size_t createTSPipeline(RenderPassInfo& rpi, Scene& s, const WindowInfo& w) {
	PipelineInfo p;
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "ts";
	p.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DNSScenePCData) + sizeof(uint32_t)};
	p.objpushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(DNSScenePCData) + sizeof(uint32_t), sizeof(glm::mat4)};
	p.vertexinputstateci = Mesh::getVISCI(VB_TRAIT_ALL, VB_TRAIT_ALL ^ (VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_NORMAL));
	p.depthtest = true;
	p.extent = w.getSCExtent();
	p.renderpass = rpi.getRenderPass();
	p.cullmode = VK_CULL_MODE_NONE;
	GH::createPipeline(p);
	Mesh::ungetVISCI(p.vertexinputstateci);
	return rpi.addPipeline(p, nullptr);
}

size_t createTSInstPipeline(RenderPassInfo& rpi, Scene& s, const WindowInfo& w) {
	PipelineInfo p;
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "tsinst";
	VkDescriptorSetLayoutBinding dtbindings[1] {{
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
		1, &dtbindings[0]
	};
	p.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DNSScenePCData) + sizeof(uint32_t)};
	p.vertexinputstateci = Mesh::getVISCI(VB_TRAIT_ALL, VB_TRAIT_ALL ^ (VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_NORMAL));
	p.depthtest = true;
	p.extent = w.getSCExtent();
	p.renderpass = rpi.getRenderPass();
	// p.cullmode = VK_CULL_MODE_BACK_BIT;
	p.cullmode = VK_CULL_MODE_NONE;
	GH::createPipeline(p);
	Mesh::ungetVISCI(p.vertexinputstateci);
	return rpi.addPipeline(p, nullptr);
}

RenderPassInfo createUIRenderPass(WindowInfo& w) {
	VkRenderPass r;
	VkAttachmentDescription a_d[1] {{
			0, 
			GH_SWAPCHAIN_IMAGE_FORMAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_LOAD,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	}};
	VkAttachmentReference a_r[1] {
		{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
	};
	GH::createRenderPass(r, 1, &a_d[0], &a_r[0], nullptr, nullptr);
	std::vector<const ImageInfo*> a_imgs = {w.getSCImages()};
	RenderPassInfo rpi(r, w.getNumSCIs(), w.getSCExtent(), {}, a_imgs, 0);

	return rpi;
}

size_t createUIPipeline(RenderPassInfo& rpi, Scene& s, const WindowInfo& w) {
	PipelineInfo p;
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "UI";
	p.renderpass = rpi.getRenderPass();
	p.extent = w.getSCExtent(); 
	p.cullmode = VK_CULL_MODE_NONE;
	p.pushconstantrange = {
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof(UIPushConstantData)
	};
	VkDescriptorSetLayoutBinding pbindings[1] {{
		0,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	}};
	p.descsetlayoutci = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1, &pbindings[0]
	};
	VkSpecializationMapEntry specmap[2] {
		{0, 0, sizeof(uint32_t)},
		{1, sizeof(uint32_t), sizeof(uint32_t)}
	};
	VkSpecializationInfo spi[2];
	spi[0] = {2, &specmap[0], sizeof(VkExtent2D), static_cast<void*>(&p.extent)};
	spi[1] = {.dataSize = 0}; // TODO: is this needed???
	p.specinfo = &spi[0];
	GH::createPipeline(p);
	return rpi.addPipeline(p, nullptr);
}

#endif

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
	ghii.dexts.push_back("VK_KHR_uniform_buffer_standard_layout"); 
	ghii.dps = {};
	ghii.dps.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64});
	ghii.dps.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8});
	VkPhysicalDeviceUniformBufferStandardLayoutFeatures ubo_std_layout;
	ubo_std_layout.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES;
	ubo_std_layout.pNext = nullptr;
	ubo_std_layout.uniformBufferStandardLayout = VK_TRUE;
	ghii.pdfeats.pNext = &ubo_std_layout;

	GH gh(ghii);
	gh.setShaderDirectory("../../resources/shaders/SPIRV/");
	WindowInitInfo wii;
	wii.msaa = VK_SAMPLE_COUNT_4_BIT;
	WindowInfo w(wii);
	Scene s(w);

#ifdef ST_TS_WIN
	wii.msaa = VK_SAMPLE_COUNT_1_BIT;
	wii.target_display = 1;
	WindowInfo ts_w(wii);
	Scene ts_s(ts_w);
#endif

	/*
	 * Renderpasses & Pipelines
	 */
	RenderPassInfo* sm_rp = createSMRenderPass(s, w);

	RenderPassInfo* main_rp = createMainRenderPass(s, w);
	size_t dnsp_idx = createDNSPipeline(*main_rp, s, w);
	size_t dnsinstp_idx = createDNSInstancedPipeline(*main_rp, s, w);
	DNSScenePCData dns_s_pc {s.getCamera()->getVP(), s.getCamera()->getPos()};
	main_rp->setScenePC(dnsp_idx, &dns_s_pc);
	main_rp->setScenePC(dnsinstp_idx, &dns_s_pc);

#ifdef ST_TS_WIN
	RenderPassInfo* ts_rp = createTSRenderPass(ts_s, ts_w);
	size_t ts_p_idx = createTSPipeline(*ts_rp, ts_s, ts_w);
	size_t tsinst_p_idx = createTSInstPipeline(*ts_rp, ts_s, ts_w);
	DNSScenePCData dns_ts_s_pc;
	ts_rp->setScenePC(ts_p_idx, &dns_ts_s_pc);
	ts_rp->setScenePC(tsinst_p_idx, &dns_ts_s_pc);
	RenderPassInfo ui_rp = createUIRenderPass(ts_w);
	size_t ui_p_idx = createUIPipeline(ui_rp, ts_s, ts_w);

	/*
	 * UI Setup
	 */
	StatsData sd;
	initStatsData(sd);

	UIHandler ui(ui_rp.getRenderSet(ui_p_idx).pipeline, ts_w.getSCExtent());
	UIContainer* stats = ui.addComponent(UIContainer());
	stats->setBGCol({0.1, 0.1, 0.1, 0.7});
	stats->setExt({600, (float)ts_w.getSCExtent().height});
	UITextInitInfo tii = {};
	tii.text = L"";
	tii.size = 24;
	tii.dpi = 144;
	sd.summary_text = stats->addChild(UIText(tii));
	sd.summary_text->setBGCol({0, 0, 0, 0});
	UIImage* tex_mon = ui.addComponent(UIImage());
	tex_mon->setPos({(float)ts_w.getSCExtent().width - 600, (float)ts_w.getSCExtent().height});
	tex_mon->setExt({600, -600});
	UIContainer* hist = ui.addComponent(UIContainer());
	hist->setBGCol({0.1, 0.1, 0.1, 0.7});
	UIContainer* hist_area = hist->addChild(UIContainer());
	hist_area->setBGCol({0.8, 0.8, 0.8, 0.7});
	const float hist_margin = 50, hist_width = 600 - 2*hist_margin, bar_width = hist_width / (float)ST_STATS_N_BINS, 
				text_width = (ST_STATS_BIN_MAX - ST_STATS_BIN_MIN) / (float)ST_STATS_N_BINS;
	float text_low = ST_STATS_BIN_MIN;
	tii.size = 12;
	for (size_t i = 0; i < ST_STATS_N_BINS; i++) {
		sd.hist_bars[i] = hist_area->addChild(UIContainer());
		sd.hist_bars[i]->setPos({(float)i * bar_width, hist_width});
		sd.hist_bars[i]->setExt({bar_width, 0});
		sd.hist_bars[i]->setBGCol({0.1, 0.7, 0.1, 1});

		tii.text = std::to_wstring(text_low).substr(0, 4);
		UIText* hist_text_temp = hist_area->addChild(UIText(tii));
		text_low += text_width;
		hist_text_temp->setPos(sd.hist_bars[i]->getPos());
		hist_text_temp->setPos(hist_text_temp->getPos() - (UICoord){hist_text_temp->getExt().x / 2, 0});
		hist_text_temp->setBGCol({0, 0, 0, 0});
	}

	UIText* hist_text_temp = hist_area->addChild(UIText(std::to_wstring(text_low).substr(0, 4)));
	text_low += text_width;
	hist_text_temp->setPos(sd.hist_bars[ST_STATS_N_BINS-1]->getPos() + (UICoord){bar_width, 0});
	hist_text_temp->setPos(hist_text_temp->getPos() - (UICoord){hist_text_temp->getExt().x / 2, 0});
	hist_text_temp->setBGCol({0, 0, 0, 0});

	hist_area->setPos({hist_margin, hist_margin});
	hist_area->setExt({hist_width, hist_width});

	hist->setPos({(float)ts_w.getSCExtent().width - (hist_width + 2*hist_margin), (float)ts_w.getSCExtent().height - (600 + hist_width + 2*hist_margin)});
	hist->setExt({hist_width + 2*hist_margin, hist_width + 2*hist_margin});
#endif

	/*
	 * This test scene is meant to be strenuous but not unfairly so.
	 * E.g., there are many meshces, but they will be instanced if feasible,
	 * textures are high-resolution, but not higher than needed
	 *
	 * Several trees are used to boost poly count and make complex shadows. TODO
	 * A player mesh is bound to the first-person camera to track near-field shadows. TODO
	 * Post-processing effects are added (although most of these should not scale with scene complexity, their algorithms may fail on a complex scene) TODO
	 */

	/*
	 * Lighting Setup
	 */
	DirectionalLightInitInfo kl_ii;
	// kl_ii.super_light.c = glm::normalize(glm::vec3(1, 1, 0.8));
	kl_ii.super_light.c = glm::normalize(glm::vec3(0.5));
	kl_ii.super_directional.f = glm::normalize(glm::vec3(-1));
	
	DirectionalLight* key_light = s.addDirectionalLight(DirectionalLight(kl_ii), {{1024, 1024}, {1024, 1024}, {1024, 1024}});
	PipelineInfo sm_pipeline = createSMPipeline(*sm_rp); // added several times to different render sets w/ different dyn viewport/scissor
	std::vector<size_t> sm_p_idxs = s.addSMPipeline(*key_light, sm_pipeline, *sm_rp, nullptr);
	for (size_t i = 0; i < sm_p_idxs.size(); i++) sm_rp->setScenePC(sm_p_idxs[i], &key_light->getSMDatum(i).getVP()); 
	PipelineInfo sminst_pipeline = createSMInstancedPipeline(*sm_rp); // added several times to different render sets w/ different dyn viewport/scissor
	std::vector<size_t> sminst_p_idxs = s.addSMPipeline(*key_light, sminst_pipeline, *sm_rp, nullptr);
	for (size_t i = 0; i < sminst_p_idxs.size(); i++) sm_rp->setScenePC(sminst_p_idxs[i], &key_light->getSMDatum(i).getVP()); 

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
	DNSObjectPCData ground_pcd = {0, ground.getModelMatrix()};
	VkDescriptorSet ds_temp;
	const TextureSet& set = th.getSet("soil");
	GH::createDS(main_rp->getRenderSet(dnsp_idx).pipeline, ds_temp);
	s.addLightCatcher(&ground, ds_temp, {0}, {}, {});
	s.addShadowCaster(&ground, {0});
	GH::updateDS(ds_temp, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, set.getTexture("diffuse").getDII(), {});
	GH::updateDS(ds_temp, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, set.getTexture("normal").getDII(), {});
	GH::updateDS(ds_temp, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, set.getTexture("specular").getDII(), {});
	main_rp->addMesh(&ground, ds_temp, &ground_pcd, dnsp_idx);
	for (size_t sm_p_i : sm_p_idxs) sm_rp->addMesh(&ground, VK_NULL_HANDLE, &ground.getModelMatrix(), sm_p_i);

	const uint8_t trees_n = 63;
	const float trees_offset = 20, trees_range = 20;
	std::vector<InstancedMeshData> trees_imdata(trees_n);
	srand(0);
	for (uint8_t i = 0; i < trees_n; i++) {
		trees_imdata[i].m = glm::mat4(1);
		trees_imdata[i].m = glm::rotate(trees_imdata[i].m, glm::two_pi<float>()*(float)rand()/(float)RAND_MAX, glm::vec3(0, 1, 0));
		trees_imdata[i].m = glm::translate(trees_imdata[i].m, glm::vec3(trees_offset, 0, 0) + trees_range * glm::vec3((float)rand() / (float)RAND_MAX, 0, 0));
		// trees_imdata[i].m = glm::rotate(trees_imdata[i].m, (float)i * 0.3f, glm::vec3(0, 1, 0));
		// trees_imdata[i].m = glm::translate(trees_imdata[i].m, glm::vec3((float)i*2, 0, 0));
	}
	InstancedMesh tree_body("resources/models/tree_body.obj", trees_imdata, VB_TRAIT_ALL);
	DNSObjectPCData tree_body_pcd = {1, tree_body.getModelMatrix()};
	GH::createDS(main_rp->getRenderSet(dnsinstp_idx).pipeline, ds_temp);
	s.addLightCatcher(&tree_body, ds_temp, {0}, {}, {});
	s.addShadowCaster(&tree_body, {0});
	GH::updateDS(ds_temp, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, set.getTexture("diffuse").getDII(), {});
	GH::updateDS(ds_temp, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, set.getTexture("normal").getDII(), {});
	GH::updateDS(ds_temp, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, set.getTexture("specular").getDII(), {});
	GH::updateDS(ds_temp, 6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {}, tree_body.getInstanceUB().getDBI());
	main_rp->addMesh(&tree_body, ds_temp, nullptr, dnsinstp_idx);
	GH::createDS(sminst_pipeline, ds_temp);
	GH::updateDS(ds_temp, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {}, tree_body.getInstanceUB().getDBI());
	for (size_t sm_p_i : sminst_p_idxs) sm_rp->addMesh(&tree_body, ds_temp, nullptr, sm_p_i);

#ifdef ST_TS_WIN
	ts_rp->addMesh(&ground, VK_NULL_HANDLE, &ground.getModelMatrix(), ts_p_idx);
	GH::createDS(ts_rp->getRenderSet(tsinst_p_idx).pipeline, ds_temp);
	GH::updateDS(ds_temp, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {}, tree_body.getInstanceUB().getDBI());
	ts_rp->addMesh(&tree_body, ds_temp, nullptr, tsinst_p_idx);
	Mesh camera_frust("../../resources/models/objs/cube.obj", VB_TRAIT_ALL);
	ts_rp->addMesh(&camera_frust, VK_NULL_HANDLE, &camera_frust.getModelMatrix(), ts_p_idx);
	ui.setTex(*tex_mon, s.getShadowAtlas(), ui_rp.getRenderSet(ui_p_idx).pipeline);
	dns_ts_s_pc.vp = key_light->getSMData()[0].getVP();
#endif

	w.addTasks(s.getDrawTasks());

#ifdef ST_TS_WIN
	ts_w.addTasks(ts_s.getDrawTasks());
	ts_w.addTask(cbRecTaskTemplate(ui_rp.getRPT()));
	ts_w.addTask(cbRecTaskTemplate([&ui, rp = ui_rp.getRenderPass(), fb = ui_rp.getFramebuffers()]
		(uint8_t scii, VkCommandBuffer& c) {
		ui.recordDraw(fb[scii], rp, c); // might need to mod fb idx against ui's own scii count
	}));
#endif
	
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
	InputData id;
	initInput(ih, s, w, id);

	s.getCamera()->setPos(player_c->getPos());
	dns_s_pc = (DNSScenePCData){s.getCamera()->getVP(), s.getCamera()->getPos()};

	ph.start();

	float theta = 0;

#ifdef ST_TS_WIN
	while (w.frameCallback() && ts_w.frameCallback()) {
#else
	while (w.frameCallback()) {
#endif
#ifdef ST_TS_WIN
		sd.frame_dones[sd.i] = SDL_GetTicks() - sd.last_TOL;
		sd.last_frame_done = SDL_GetTicks();
#endif
		id.movementdir = glm::vec3(0);
		ih.update();
		SDL_PumpEvents();
		if (id.movementdir != glm::vec3(0) && glm::length(player_c->getVel()) < MOVEMENT_CAP) {
			id.movementdir = glm::normalize(id.movementdir);
			ph.addTimedForce({player_c, MOVEMENT_SENS * id.movementdir, 0});
		}

		ph.update();

		s.getCamera()->setPos(player_c->getPos());
		s.getCamera()->updateView();
		s.getCamera()->updateProj();
		dns_s_pc = (DNSScenePCData){s.getCamera()->getVP(), s.getCamera()->getPos()};
		camera_frust.setModelMatrix(glm::inverse(s.getCamera()->getVP()));
		/*
		updateCascade(s, *key_light, 0);
		updateCascade(s, *key_light, 1);
		updateCascade(s, *key_light, 2);
		*/
		s.updateSMDCascade(*key_light, 0, glm::vec2(0, 0.21));
		s.updateSMDCascade(*key_light, 1, glm::vec2(0.2, 0.31));
		s.updateSMDCascade(*key_light, 2, glm::vec2(0.3, 1));

#ifdef ST_TS_WIN
		dns_ts_s_pc.vp = key_light->getSMData()[0].getVP();
		sd.TOLs[sd.i] = SDL_GetTicks() - sd.last_frame_done;
		sd.last_TOL = SDL_GetTicks();
		updateStatsData(sd);
#endif
	}

	vkQueueWaitIdle(GH::getGenericQueue());

#ifdef ST_TS_WIN
	ui_rp.destroy();
#endif

	return 0;
	}

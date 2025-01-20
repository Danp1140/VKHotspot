#include "Scene.h"
RenderPassInfo::RenderPassInfo(
	VkRenderPass r, 
	const uint32_t nsci,
	const ImageInfo* scis,
	const ImageInfo* d,
	std::vector<VkClearValue>&& c) : 
		renderpass(r), 
		numscis(nsci),
		clears(c) {
	extent = scis ? scis[0].extent : d->extent;
	createFBs(nsci, scis, d);
}

void RenderPassInfo::destroy() {
	for (RenderSet r : rendersets) GH::destroyPipeline(r.pipeline);
	if (framebuffers) {
		for (uint8_t scii = 0; scii < numscis; scii++) vkDestroyFramebuffer(GH::getLD(), framebuffers[scii], nullptr);
		delete[] framebuffers;
	}
	if (renderpass != VK_NULL_HANDLE) GH::destroyRenderPass(renderpass);
}

void RenderPassInfo::addPipeline(const PipelineInfo& p, const void* pcd) {
	rendersets.push_back({p, {}, {}, {}, nullptr, pcd});
}

void RenderPassInfo::addMesh(const MeshBase* m, VkDescriptorSet ds, const void* pc, size_t pidx) {
	rendersets[pidx].meshes.push_back(m);
	rendersets[pidx].objdss.push_back(ds);
	rendersets[pidx].objpcdata.push_back(pc);
}

void RenderPassInfo::setUI(const UIHandler* u, size_t pidx) {
	rendersets[pidx].ui = u;
}

std::vector<cbRecTaskTemplate> RenderPassInfo::getTasks() const {
	std::vector<cbRecTaskTemplate> tasks;
	tasks.emplace_back(getRPT());
	size_t counter;
#ifdef VKH_VERBOSE_DRAW_TASKS
	std::cout << "RenderPassInfo " << this << " tasks {" << std::endl;
#endif
	for (const RenderSet& r : rendersets) {
#ifdef VKH_VERBOSE_DRAW_TASKS
		std::cout << "RenderSet " << &r << " tasks {" << std::endl;
#endif
		// can't do pipeline binds out here ;-;
		// could add a setting that puts entire pipeline & all mesh records in one 2ary cb
		// this would be more efficient for meshes that aren't swapped in and out frequently
		// okay we could probably make that work by moving the mesh iteration within the emplace func
		// and starting the secondary cb in the emplace but not in the MeshBase::recordDraw
		counter = 0;
		for (const MeshBase* m : r.meshes) {
#ifdef VKH_VERBOSE_DRAW_TASKS
			std::cout << "Mesh " << &m << std::endl;
#endif
			tasks.emplace_back(
				[m, r, &rp = renderpass, &fb = framebuffers, counter, ns = numscis] 
				(uint8_t scii, VkCommandBuffer& c) {
				// a little bit of an odd impl, but allows for mismatch between framebuffer scis and
				// window scis
				m->recordDraw(fb[scii % ns], rp, r, counter, c);
			});
			counter++;
		}
		if (r.ui) {
#ifdef VKH_VERBOSE_DRAW_TASKS
			std::cout << "UI " << r.ui << std::endl;
#endif
			tasks.emplace_back(
				[ui = r.ui, &rp = renderpass, &fb = framebuffers, ns = numscis]
				(uint8_t scii, VkCommandBuffer& c) {
				ui->recordDraw(fb[scii % ns], rp, c);
			});
		}
#ifdef VKH_VERBOSE_DRAW_TASKS
		std::cout << "}" << std::endl;
#endif
	}
#ifdef VKH_VERBOSE_DRAW_TASKS
	std::cout << "}" << std::endl;
#endif
	return tasks;
}

void RenderPassInfo::createFBs(const uint32_t nsci, const ImageInfo* scis, const ImageInfo* d) {
	framebuffers = new VkFramebuffer[nsci];
	uint8_t nattach = (scis ? 1 : 0) + (d ? 1 : 0);
	VkImageView attachments[nattach];
	if (d) attachments[nattach - 1] = d->view;
	VkFramebufferCreateInfo framebufferci {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		nullptr,
		0,
		renderpass,
		nattach, &attachments[0],
		extent.width, extent.height, 1
	};
	for (uint8_t scii = 0; scii < nsci; scii++) {
		if (scis) attachments[0] = scis[scii].view;
		vkCreateFramebuffer(GH::getLD(), &framebufferci, nullptr, &framebuffers[scii]);
	}
}

// could p be inline/constexpr
cbRecTaskRenderPassTemplate RenderPassInfo::getRPT() const {
	return cbRecTaskRenderPassTemplate(renderpass, framebuffers, numscis, extent, clears.size(), clears.data());
}

Scene::Scene(float a) : 
	numdirlights(0), 
	numdirsclights(0),
	numcatchers(0) {
	camera = new Camera(glm::vec3(10, 8, 10), glm::vec3(-5, -4, -5), glm::quarter_pi<float>(), a);
	lightub.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	lightub.size = sizeof(LUBData);
	GH::createBuffer(lightub);
}

Scene::~Scene() {
	if (lightub.buffer != VK_NULL_HANDLE) GH::destroyBuffer(lightub);
	delete camera;
	for (RenderPassInfo* r : renderpasses) {
		r->destroy();
		delete r;
	}
}

std::vector<cbRecTaskTemplate> Scene::getDrawTasks() {
	std::vector<cbRecTaskTemplate> result;
	std::vector<cbRecTaskTemplate> temp;
	for (const RenderPassInfo* r : renderpasses) {
		temp = r->getTasks();
		result.insert(result.end(), temp.begin(), temp.end());
	}
	return result;
}

RenderPassInfo* Scene::addRenderPass(const RenderPassInfo& r) {
	renderpasses.push_back(new RenderPassInfo(r));
	return renderpasses.back();
}

DirectionalLight* Scene::addDirectionalLight(DirectionalLight&& l) {
	LUBLightEntry tempe;
	if (l.getShadowMap().image == VK_NULL_HANDLE) {
		if (numdirlights == SCENE_MAX_DIR_LIGHTS) {
			WarningError("Maximum directional lights exceeded, not adding\n").raise();
			return nullptr;
		}
		dirlights[numdirlights] = std::move(l);
		numdirlights++;
		
		tempe.vp = Light::smadjmat * dirlights[numdirlights - 1].getVP();
		tempe.p = dirlights[numdirlights - 1].getPos();
		tempe.c = dirlights[numdirlights - 1].getCol();
		GH::updateBuffer(lightub, &tempe, sizeof(LUBLightEntry), sizeof(LUBLightEntry) * (numdirlights - 1));

		return &dirlights[numdirlights - 1];
	}

	if (numdirsclights == SCENE_MAX_DIR_SHADOWCASTING_LIGHTS) {
		WarningError("Maximum directional shadowcasting lights exceeded, not adding\n").raise();
		return nullptr;
	}
	dirsclights[numdirsclights] = std::move(l);
	numdirsclights++;

	tempe.vp = Light::smadjmat * dirsclights[numdirsclights - 1].getVP();
	tempe.p = dirsclights[numdirsclights - 1].getPos();
	tempe.c = dirsclights[numdirsclights - 1].getCol();
	GH::updateBuffer(lightub, &tempe, sizeof(LUBLightEntry), offsetof(LUBData, scdle) + sizeof(LUBLightEntry) * (numdirsclights - 1));

	VkRenderPass r;
	VkAttachmentDescription attachdesc {
		0, 
		LIGHT_SHADOW_MAP_FORMAT,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
	};
	VkAttachmentReference attachref {0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
	GH::createRenderPass(r, 1, &attachdesc, nullptr, &attachref);

	RenderPassInfo* rpi = new RenderPassInfo(r, 1, nullptr, &dirsclights[numdirsclights - 1].getShadowMap(), {{1, 0}});

	renderpasses.insert(renderpasses.begin(), rpi); 

	return &dirsclights[numdirsclights - 1];
}

void Scene::hookupShadowCatcher(const Mesh* m, VkDescriptorSet& ds, std::vector<uint32_t> dlidxs, std::vector<uint32_t> scdlidxs) {
	size_t cuboffset = offsetof(LUBData, ce);
	if (numcatchers + 1 == SCENE_MAX_SHADOWCATCHERS) {
		WarningError("Max shadowcatchers reached, not adding").raise();
		return;
	}
	numcatchers++;
	LUBCatcherEntry tempe;
	tempe.nscdls = scdlidxs.size();
	memcpy(
		&tempe.scdlidxs[0], 
		scdlidxs.data(), 
		sizeof(uint32_t) * std::min(tempe.nscdls, (uint32_t)SCENE_MAX_DIR_SHADOWCASTING_LIGHTS));
	tempe.ndls = dlidxs.size();
	memcpy(
		&tempe.dlidxs[0], 
		dlidxs.data(), 
		sizeof(uint32_t) * std::min(tempe.ndls, (uint32_t)SCENE_MAX_DIR_LIGHTS));
	GH::updateBuffer(lightub, &tempe, sizeof(LUBCatcherEntry), cuboffset + sizeof(LUBCatcherEntry) * (numcatchers - 1));

	GH::updateDS(ds, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {}, {lightub.buffer, 0, cuboffset});
	std::vector<VkDescriptorImageInfo> ii;
	for (uint8_t i = 0; i < numdirsclights; i++) 
		ii.push_back(dirsclights[i].getShadowMap().getDII());
	for (uint8_t i = numdirsclights; i < SCENE_MAX_DIR_SHADOWCASTING_LIGHTS; i++) 
		ii.push_back(UIHandler::uiToGHImageInfo(UIComponent::getNoTex()).getDII());
	GH::updateArrayDS(ds, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, std::move(ii));
	GH::updateDS(ds, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {}, {lightub.buffer, cuboffset + sizeof(LUBCatcherEntry) * (numcatchers - 1), sizeof(LUBCatcherEntry)});
	// TODO: also add mesh's AABB
}

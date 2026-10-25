#include "Scene.h"

void RenderSet::addMesh(const MeshBase* m, VkDescriptorSet ds, const void* pc) {
	meshes.push_back(m);
	objdss.push_back(ds);
	objpcdata.push_back(pc);
}

RenderPassInfo::RenderPassInfo(
	VkRenderPass r, 
	const uint32_t nsci,
	const ImageInfo* scis,
	const ImageInfo* ms,
	const ImageInfo* d,
	std::vector<VkClearValue>&& c) : 
		renderpass(r), 
		numscis(nsci),
		clears(c),
		cull_map(nullptr) {
	extent = d ? d->extent : (ms ? ms->extent : scis[0].extent);
	// TODO: fix up createFBs to make more sense and perhaps be more flexible with attachment order
	// nsci should really be called something else
	createFBs(nsci, scis, ms, d);
}

RenderPassInfo::RenderPassInfo(VkRenderPass r, const uint32_t nsci, VkExtent2D ext, std::vector<VkClearValue>&& c, std::vector<const ImageInfo*> att, uint8_t sci_att_idx) :
	renderpass(r), numscis(nsci), extent(ext), clears(c), cull_map(nullptr) {
	createFBs(att, sci_att_idx);
}

RenderPassInfo::RenderPassInfo(RGNodeImage& r) : 
	renderpass(r.getRP()->rp),
	framebuffers(r.getRP()->fb),
	extent(r.getImage()[0].extent),
	clears(r.getRP()->clears, r.getRP()->clears + r.getRP()->n_attach),
	numscis(r.getNImages()),
	cull_map(nullptr) {}

void RenderPassInfo::destroy() {
	std::set<VkPipeline> destroyed;
	for (RenderSet r : rendersets) {
		if (!destroyed.contains(r.pipeline.pipeline)) {
			GH::destroyPipeline(r.pipeline);
			destroyed.insert(r.pipeline.pipeline);
		}
	}
	if (framebuffers) {
		if (framebuffers[0] == framebuffers[1]) vkDestroyFramebuffer(GH::getLD(), framebuffers[0], nullptr);
		else {
			for (uint8_t scii = 0; scii < numscis; scii++) vkDestroyFramebuffer(GH::getLD(), framebuffers[scii], nullptr);
		}
		delete[] framebuffers;
	}
	if (renderpass != VK_NULL_HANDLE) GH::destroyRenderPass(renderpass);
}

size_t RenderPassInfo::addPipeline(const PipelineInfo& p, const void* pcd) {
	rendersets.push_back({p, {}, {}, {}, pcd});
	return rendersets.size() - 1;
}

size_t RenderPassInfo::addPipeline(const PipelineInfo& p, const void* pcd, VkViewport vp, VkRect2D sc) {
	rendersets.push_back({p, {}, {}, {}, pcd, vp, sc});
	return rendersets.size() - 1;
}

void RenderPassInfo::addMesh(const MeshBase* m, VkDescriptorSet ds, const void* pc, size_t pidx) {
	rendersets[pidx].addMesh(m, ds, pc);
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
			std::cout << "Mesh " << m << std::endl;
#endif
			if (cull_map && cull_map->contains(m)) {
				tasks.emplace_back(
					[m, r, &rp = renderpass, &fb = framebuffers, counter, ns = numscis, cm = cull_map] 
					(uint8_t scii, VkCommandBuffer& c) {
					// a little bit of an odd impl, but allows for mismatch between framebuffer scis and
					// window scis
					if (!(*cm)[m]) std::cout << "culling mesh\n";
					if (!(*cm)[m]) return false;
					m->recordDraw(fb[scii % ns], rp, r, counter, c);
					return true;
				});
			}
			else {
				tasks.emplace_back(
					[m, r, &rp = renderpass, &fb = framebuffers, counter, ns = numscis] 
					(uint8_t scii, VkCommandBuffer& c) {
					// a little bit of an odd impl, but allows for mismatch between framebuffer scis and
					// window scis
					m->recordDraw(fb[scii % ns], rp, r, counter, c);
					return true;
				});

			}
			counter++;
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

void RenderPassInfo::createFBs(const uint32_t nsci, const ImageInfo* scis, const ImageInfo* r, const ImageInfo* d) {
	// to make msaa easily compatible with non-msaa, we could still have nsci framebuffers,
	// but just have them all be the same framebuffer in an msaa context
	framebuffers = new VkFramebuffer[nsci];
	const uint8_t nattach = (scis ? 1 : 0) + (r ? 1 : 0) + (d ? 1 : 0);
	VkImageView attachments[nattach];
	const uint8_t scattachidx = r ? 2 : 0;
	const uint8_t dattachidx = r ? 1 : (scis ? 1 : 0);
	if (d) attachments[dattachidx] = d->view;
	if (r) attachments[0] = r->view;
	VkFramebufferCreateInfo framebufferci {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		nullptr,
		0,
		renderpass,
		nattach, &attachments[0],
		extent.width, extent.height, 1
	};
	for (uint8_t scii = 0; scii < nsci; scii++) {
		if (scis) attachments[scattachidx] = scis[scii].view;
		vkCreateFramebuffer(GH::getLD(), &framebufferci, nullptr, &framebuffers[scii]);
	}
}

void RenderPassInfo::createFBs(const std::vector<const ImageInfo*>& imgs, uint32_t sci_attach_idx) {
	framebuffers = new VkFramebuffer[numscis];
	VkImageView attachments[imgs.size()];
	for (size_t i = 0; i < imgs.size(); i++) {
		if (i != sci_attach_idx) attachments[i] = imgs[i]->view;
	}
	VkFramebufferCreateInfo framebufferci {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		nullptr,
		0,
		renderpass,
		static_cast<uint32_t>(imgs.size()), &attachments[0],
		extent.width, extent.height, 1
	};
	for (uint8_t scii = 0; scii < numscis; scii++) {
		attachments[sci_attach_idx] = imgs[sci_attach_idx][scii].view;
		vkCreateFramebuffer(GH::getLD(), &framebufferci, nullptr, &framebuffers[scii]);
	}
}

/*
 * imgs must be passed in in the order of the attachments in the renderpass
 * imgs[sciidx] must be an array of the scis
 */
void RenderPassInfo::createFBs(const uint32_t nsci, uint32_t sciidx, const std::vector<const ImageInfo*> imgs) {
	// to make msaa easily compatible with non-msaa, we could still have nsci framebuffers,
	// but just have them all be the same framebuffer in an msaa context
	framebuffers = new VkFramebuffer[nsci];
	VkImageView attachments[imgs.size()];
	for (uint8_t i = 0; i < imgs.size(); i++) {
		if (i != sciidx) attachments[i] = imgs[i]->view;
	}
	VkFramebufferCreateInfo framebufferci {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		nullptr,
		0,
		renderpass,
		(uint32_t)imgs.size(), &attachments[0],
		extent.width, extent.height, 1
	};
	for (uint8_t scii = 0; scii < nsci; scii++) {
		if (sciidx != -1u) attachments[sciidx] = imgs[sciidx][scii].view;
		vkCreateFramebuffer(GH::getLD(), &framebufferci, nullptr, &framebuffers[scii]);
	}
}

// could p be inline/constexpr
cbRecTaskRenderPassTemplate RenderPassInfo::getRPT() const {
	std::cout << "getRPT numscis: " << (int)numscis << std::endl;
	return cbRecTaskRenderPassTemplate(renderpass, framebuffers, numscis, extent, clears.size(), clears.data());
}

RPNode::~RPNode() {
	// if (fb) delete fb;
	// if (clears) delete clears;
}

RPNode* RGNodeDraw::visit(VkExtent2D ext) {
	if (!rp) {
		createRP(ext);

		for (RGNodeImage* d : dsts) d->setRP(rp);
	}
	return rp;
}

void RGNodeDraw::createRP(VkExtent2D ext) {
	rp = new RPNode();

	uint8_t n_fbs = 1;
	VkAttachmentDescription ads[dsts.size()];
	std::vector<VkAttachmentReference> ars;
	VkAttachmentReference depth_ar;
	depth_ar.layout = VK_IMAGE_LAYOUT_UNDEFINED;
	int sc_idx = -1;
	for (size_t dst_i = 0; dst_i < dsts.size(); dst_i++) {
		ads[dst_i].flags = 0;
		ads[dst_i].format = dsts[dst_i]->getImage()[0].format;
		ads[dst_i].samples = VK_SAMPLE_COUNT_1_BIT;
		ads[dst_i].loadOp = dsts[dst_i]->shouldClear() ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		ads[dst_i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		ads[dst_i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		ads[dst_i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		ads[dst_i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		ads[dst_i].finalLayout = dsts[dst_i]->getDstLayout();
		if (dsts[dst_i]->getNImages() > 1) {
			if (n_fbs == 1) {
				n_fbs = dsts[dst_i]->getNImages();
				sc_idx = dst_i;
			}
			else {
				FatalError("Only one swapchain allowed per RP tree").raise();
			}
		}
		if (dsts[dst_i]->getFlags() & RG_FLAG_DEPTH_BIT) {
			if (depth_ar.layout != VK_IMAGE_LAYOUT_UNDEFINED)
				FatalError("Only one depth buffer allowed").raise();
			depth_ar.attachment = static_cast<uint32_t>(dst_i);
			depth_ar.layout = dsts[dst_i]->getSrcLayout();
		}
		else 
			ars.push_back({static_cast<uint32_t>(dst_i), dsts[dst_i]->getSrcLayout()});
	}

	VkSubpassDescription subpass;
	subpass.flags = 0;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.inputAttachmentCount = 0;
	if (ars.size() > 0) {
		subpass.colorAttachmentCount = static_cast<uint32_t>(ars.size());
		subpass.pColorAttachments = ars.data();
	}	
	else subpass.colorAttachmentCount = 0;
	subpass.pResolveAttachments = 0;
	if (depth_ar.layout != VK_IMAGE_LAYOUT_UNDEFINED)
		subpass.pDepthStencilAttachment = &depth_ar;	
	else subpass.pDepthStencilAttachment = nullptr;	
	subpass.preserveAttachmentCount = 0;
	VkSubpassDependency sp_dep;
	sp_dep.srcSubpass = VK_SUBPASS_EXTERNAL;
	sp_dep.dstSubpass = 0;
	sp_dep.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	sp_dep.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	sp_dep.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	sp_dep.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;	
	sp_dep.dependencyFlags = 0;	

	VkRenderPassCreateInfo rp_ci {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		nullptr,
		0,
		static_cast<uint32_t>(dsts.size()), &ads[0],
		1, &subpass,
		1, &sp_dep
	};
	vkCreateRenderPass(GH::getLD(), &rp_ci, nullptr, &rp->rp);

	rp->n_attach = dsts.size();
	rp->fb = new VkFramebuffer[n_fbs];
	rp->clears = new VkClearValue[rp->n_attach];
	VkImageView fb_attach[rp->n_attach];
	for (size_t dst_i = 0; dst_i < dsts.size(); dst_i++) {
		if (dst_i != sc_idx) {
			dsts[dst_i]->tryMakeImg(ext);
			fb_attach[dst_i] = dsts[dst_i]->getImage()[0].view;
		}
		if (dsts[dst_i]->shouldClear()) 
			rp->clears[dst_i] = dsts[dst_i]->getClear();
	}
	
	VkFramebufferCreateInfo fb_ci {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		nullptr,
		0,
		rp->rp,
		static_cast<uint32_t>(dsts.size()), &fb_attach[0],
		dsts[0]->getImage()[0].extent.width, dsts[0]->getImage()[0].extent.height, 1
	};
	
	for (uint8_t fb_i = 0; fb_i < n_fbs; fb_i++) {
		if (sc_idx != -1) 
			fb_attach[sc_idx] = dsts[sc_idx]->getImage()[fb_i].view;
		vkCreateFramebuffer(GH::getLD(), &fb_ci, nullptr, &rp->fb[fb_i]);
	}
}

RGNodeImage::RGNodeImage(const RGNodeImageInitInfo& ii) :
		srcs(ii.srcs),
		src_layout(ii.src_layout),
		dst_layout(ii.dst_layout),
		should_clear(ii.should_clear) {
	if (should_clear) clear = ii.clear;
}

RPNode* RGNodeImage::visit(VkExtent2D ext) {
	if (!rp) {	
		for (RGNode* s : srcs) {
			deps.insert(s->visit(ext)->rp);
		}

		if (!rp || rp->rp == VK_NULL_HANDLE) {
			createRP(ext);
		}
	}
	return rp;
}

void RGNodeImage::createRP(VkExtent2D ext) {
	rp = new RPNode();

	VkAttachmentDescription ad;
	VkAttachmentReference ar;

	ad.flags = 0;
	ad.format = getImage()[0].format;
	ad.samples = VK_SAMPLE_COUNT_1_BIT;
	ad.loadOp = should_clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	ad.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	ad.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	ad.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ad.finalLayout = getDstLayout();

	ar.attachment = 0;
	ar.layout = getSrcLayout();

	VkSubpassDescription subpass;
	subpass.flags = 0;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.inputAttachmentCount = 0;
	if (getFlags() & RG_FLAG_DEPTH_BIT) {
		subpass.colorAttachmentCount = 0;
		subpass.pDepthStencilAttachment = &ar;	
	}
	else {
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &ar;
		subpass.pDepthStencilAttachment = nullptr;	
	}	
	subpass.pResolveAttachments = 0;
	subpass.preserveAttachmentCount = 0;
	VkSubpassDependency sp_dep;
	sp_dep.srcSubpass = VK_SUBPASS_EXTERNAL;
	sp_dep.dstSubpass = 0;
	sp_dep.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	sp_dep.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	sp_dep.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	sp_dep.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;	
	sp_dep.dependencyFlags = 0;	

	VkRenderPassCreateInfo rp_ci {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		nullptr,
		0,
		1, &ad,
		1, &subpass,
		1, &sp_dep
	};
	vkCreateRenderPass(GH::getLD(), &rp_ci, nullptr, &rp->rp);
	
	createFBs(ext);
}

RGNodeSample::RGNodeSample(const RGNodeSampleInitInfo& ii) :
		RGNodeImage(ii.super_image),
		flags(ii.flags),
		img(ii.img) {
	if (img.image == VK_NULL_HANDLE) {
		img_owned = true;
	}
	else {
		img_owned = false;
	}
}

RGNodeSample::~RGNodeSample() {
	if (img_owned && img.image != VK_NULL_HANDLE) GH::destroyImage(img);
}

bool RGNodeSample::tryMakeImg(VkExtent2D ext) {
	if (img.image != VK_NULL_HANDLE) return false;
	img.sampler = GH::getNearestSampler();
	img.layout = src_layout;
	img.extent = ext;
	GH::createImage(img);
	img_owned = true;
	return true;
}

void RGNodeSample::createFBs(VkExtent2D ext) {
	uint8_t n_fbs = 1;
	rp->n_attach = 1;
	rp->fb = new VkFramebuffer[n_fbs];
	rp->clears = new VkClearValue[rp->n_attach];
	if (img.image == VK_NULL_HANDLE) {
		img.sampler = GH::getNearestSampler();
		img.layout = src_layout;
		img.extent = ext;
		GH::createImage(img);
		img_owned = true;
	}
	rp->clears[0] = clear;

	VkImageView fb_attach = img.view;
	
	VkFramebufferCreateInfo fb_ci {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		nullptr,
		0,
		rp->rp,
		1, &fb_attach,
		ext.width, ext.height, 1
	};
	
	vkCreateFramebuffer(GH::getLD(), &fb_ci, nullptr, &rp->fb[0]);
}

RGNodeSwapchain::RGNodeSwapchain(const RGNodeSwapchainInitInfo& ii) : 
		RGNodeImage(ii.super_image),
		window(ii.w) {}

RPNode* RGNodeSwapchain::getRPTree() {
	return visit(window->getSCExtent());
}

void RGNodeSwapchain::createFBs(VkExtent2D ext) {
	uint8_t n_fbs = window->getNumSCIs();
	rp->n_attach = 1;
	rp->fb = new VkFramebuffer[n_fbs];
	rp->clears = new VkClearValue[rp->n_attach];
	VkImageView fb_attach;
	if (should_clear) rp->clears[0] = clear;
	
	VkFramebufferCreateInfo fb_ci {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		nullptr,
		0,
		rp->rp,
		1, &fb_attach,
		ext.width, ext.height, 1
	};
	
	for (uint8_t fb_i = 0; fb_i < n_fbs; fb_i++) {
		fb_attach = window->getSCImages()[fb_i].view;
		vkCreateFramebuffer(GH::getLD(), &fb_ci, nullptr, &rp->fb[fb_i]);
	}
}

Scene::Scene(float a) : 
	n_dir_lights(0), 
	n_spot_lights(0), 
	n_point_lights(0), 
	n_sc_lights(0),
	n_sc_dir_lights(0),
	n_sc_spot_lights(0),
	n_catchers(0),
	sa_offset({0, 0}) {
	camera = new Camera(); // TODO: consider making this no longer a pointer, and even allowing multiple cameras
	lightub.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	lightub.size = sizeof(LUBData);
	GH::createBuffer(lightub);
	GH_LOG_RESOURCE_SIZE(lightub, lightub.size)	
	
	shadow_atlas.extent = {2048, 2048};
	shadow_atlas.format = LIGHT_SHADOW_MAP_FORMAT;
	shadow_atlas.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	// shadow_atlas.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	shadow_atlas.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	vkCreateSampler(GH::getLD(), &Light::defaultshadowsamplerci, nullptr, &shadow_atlas.sampler); 
	GH::createImage(shadow_atlas);
	GH_LOG_RESOURCE_SIZE(shadow_atlas, (shadow_atlas.extent.width * shadow_atlas.extent.height * shadow_atlas.getPixelSize()))
}

Scene::~Scene() {
	if (shadow_atlas.sampler != VK_NULL_HANDLE) vkDestroySampler(GH::getLD(), shadow_atlas.sampler, nullptr);
	if (shadow_atlas.image != VK_NULL_HANDLE) GH::destroyImage(shadow_atlas);
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

LightIndex Scene::getSMSlots(LightIndex n_sms) {
	if (n_sc_lights + n_sms > SCENE_MAX_SC_LIGHTS) 
		FatalError("Too many shadowcasting lights").raise();
	return n_sc_lights;
}

DirectionalLight* Scene::addDirectionalLight(const DirectionalLight& l, const std::vector<VkExtent2D>& sm_exts) {
	if (n_dir_lights == SCENE_MAX_DIR_LIGHTS) {
		WarningError("Maximum directional lights exceeded, not adding\n").raise();
		return nullptr;
	}
	dir_lights[n_dir_lights] = l;
	DirectionalLight& added = dir_lights[n_dir_lights];
	LightIndex first_sm_idx = getSMSlots(sm_exts.size());

	glm::vec4 vectors[2];
	vectors[0] = glm::vec4(added.getCol().x, added.getCol().y, added.getCol().z, (float)sm_exts.size());
	vectors[1] = glm::vec4(added.getForward().x, added.getForward().y, added.getForward().z, (float)first_sm_idx);
	GH::updateBuffer(
		lightub, 
		&vectors[0], 
		2*sizeof(glm::vec4), 
		offsetof(LUBData, vectors) + n_dir_lights * 2 * sizeof(glm::vec4));

	if (sm_exts.size() > 0) {
		for (size_t sm_i = 0; sm_i < sm_exts.size(); sm_i++) {
			if (n_sc_lights + 1 == SCENE_MAX_SC_LIGHTS) {
				WarningError("Maximum shadowmaps lights exceeded, not adding\n").raise();
				break;
			}
			LightSMData smd;
			smd.setExtent(sm_exts[sm_i]);
			smd.setOffset(getSAZone(smd.getExtent()));
			smd.setSM(&shadow_atlas);
			added.addSMData(smd);

			updateDirSMD(n_dir_lights, first_sm_idx + sm_i, sm_i);	
		}
		n_sc_lights += sm_exts.size();
	}
	n_dir_lights++;
	GH::updateBuffer(lightub, &n_dir_lights, sizeof(uint32_t), offsetof(LUBData, light_counts));

	return &added;
}

SpotLight* Scene::addSpotLight(const SpotLight& l, const std::vector<VkExtent2D>& sm_exts) {
	if (n_spot_lights == SCENE_MAX_SPOT_LIGHTS) {
		WarningError("Maximum spot lights exceeded, not adding\n").raise();
		return nullptr;
	}
	spot_lights[n_spot_lights] = l;
	SpotLight& added = spot_lights[n_spot_lights];
	LightIndex first_sm_idx = getSMSlots(sm_exts.size());

	glm::vec4 vectors[3];
	vectors[0] = glm::vec4(added.getCol().x, added.getCol().y, added.getCol().z, (float)sm_exts.size());
	vectors[1] = glm::vec4(added.getForward().x, added.getForward().y, added.getForward().z, (float)first_sm_idx);
	vectors[2] = glm::vec4(added.getPos().x, added.getPos().y, added.getPos().z, added.getFOVY());
	GH::updateBuffer(
		lightub, 
		&vectors[0], 
		3*sizeof(glm::vec4), 
		offsetof(LUBData, vectors) + (SCENE_MAX_DIR_LIGHTS * 2 + n_spot_lights * 3) * sizeof(glm::vec4));

	if (sm_exts.size() > 0) {
		for (size_t sm_i = 0; sm_i < sm_exts.size(); sm_i++) {
			if (n_sc_lights + 1 == SCENE_MAX_SC_LIGHTS) {
				WarningError("Maximum shadowmaps lights exceeded, not adding\n").raise();
				break;
			}
			LightSMData smd;
			smd.setExtent(sm_exts[sm_i]);
			smd.setOffset(getSAZone(smd.getExtent()));
			smd.setSM(&shadow_atlas);
			added.addSMData(smd);

			updateSpotSMD(n_spot_lights, first_sm_idx + sm_i, sm_i);

		}
		n_sc_lights += sm_exts.size();
	}

	n_spot_lights++;
	GH::updateBuffer(
		lightub, 
		&n_spot_lights, 
		sizeof(uint32_t), 
		offsetof(LUBData, light_counts) + sizeof(float));	

	return &added;
}

PointLight* Scene::addPointLight(const PointLight& l) {
	if (n_point_lights == SCENE_MAX_POINT_LIGHTS) {
		WarningError("Maximum point lights exceeded, not adding\n").raise();
		return nullptr;
	}
	point_lights[n_point_lights] = l;
	PointLight& added = point_lights[n_point_lights];

	glm::vec4 vectors[2];
	vectors[0] = glm::vec4(added.getCol().x, added.getCol().y, added.getCol().z, 0);
	vectors[1] = glm::vec4(added.getPos().x, added.getPos().y, added.getPos().z, 0);
	GH::updateBuffer(
		lightub, 
		&vectors[0], 
		2*sizeof(glm::vec4), 
		offsetof(LUBData, vectors) + SCENE_MAX_DIR_LIGHTS * 2 * sizeof(glm::vec4) + SCENE_MAX_SPOT_LIGHTS * 3 * sizeof(glm::vec4) + n_point_lights * 2 * sizeof(glm::vec4));

	// no shadowmaps for now

	n_point_lights++;
	GH::updateBuffer(lightub, &n_point_lights, sizeof(uint32_t), offsetof(LUBData, light_counts) + 2 * sizeof(float));	

	return &added;
}

std::vector<size_t> Scene::addSMPipeline(const Light& l, const PipelineInfo& p, RenderPassInfo& rpi, const void* pcd) {
	std::vector<size_t> res;
	for (const LightSMData& smd : l.getSMData()) {
		res.push_back(rpi.addPipeline(p, pcd, smd.getViewport(), smd.getScissor()));
	}
	return res;
}

void Scene::addShadowCaster(
	const MeshBase* m, 
	const std::vector<uint32_t>& dl_idxs,
	const std::vector<uint32_t>& sl_idxs,
	const std::vector<uint32_t>& pl_idxs) {
	glm::vec3 temp;
	for (uint8_t v_idx = 0; v_idx < 8; v_idx++) { 
		temp = ProjectionBase::apply(m->getModelMatrix(), glm::vec3(
			m->getAABB()[v_idx % 2].x, 
			m->getAABB()[(uint8_t)floor(v_idx/2) % 2].y, 
			m->getAABB()[(uint8_t)floor(v_idx/4) % 2].z));
		for (uint8_t i = 0; i < dl_idxs.size(); i++) {
			for (uint8_t k = 0; k < dir_lights[dl_idxs[i]].getSMData().size(); k++) 
				dir_lights[dl_idxs[i]].getSMDatum(k).addVecToFocus(temp);
		}
		for (uint8_t i = 0; i < sl_idxs.size(); i++) {
			for (uint8_t k = 0; k < spot_lights[sl_idxs[i]].getSMData().size(); k++) 
				spot_lights[sl_idxs[i]].getSMDatum(k).addVecToFocus(temp);
		}
		// TODO: impl for point lights
	}
}

uint32_t Scene::addLightCatcher(
	const MeshBase* m, 
	const VkDescriptorSet& ds, 
	const std::vector<uint32_t>& d_l_idxs, 
	const std::vector<uint32_t>& s_l_idxs, 
	const std::vector<uint32_t>& p_l_idxs) {
	if (n_catchers + 1 == SCENE_MAX_SHADOW_CATCHERS) {
		WarningError("Max shadowcatchers reached, not adding").raise();
		return -1u;
	}

	if (ds != VK_NULL_HANDLE) {
		GH::updateDS(ds, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {}, {lightub.buffer, 0, offsetof(LUBData, catcher_entries)});
		GH::updateDS(ds, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, shadow_atlas.getDII(), {});
		GH::updateDS(ds, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {}, {lightub.buffer, offsetof(LUBData, catcher_entries), SCENE_MAX_SHADOW_CATCHERS *  sizeof(CatcherEntry)});
	}
	
	updateLightCatcher(m, ds, d_l_idxs, s_l_idxs, p_l_idxs, n_catchers);

	return n_catchers++;
}

void Scene::updateLightCatcher(
		const MeshBase* m, 
		const VkDescriptorSet& ds, 
		const std::vector<uint32_t>& d_l_idxs, 
		const std::vector<uint32_t>& s_l_idxs, 
		const std::vector<uint32_t>& p_l_idxs,
		uint32_t cidx) {
	CatcherEntry entry;

	entry.light_counts[0] = d_l_idxs.size();
	entry.light_counts[1] = s_l_idxs.size();
	entry.light_counts[2] = p_l_idxs.size();
	memcpy(&entry.dir_light_idxs[0], d_l_idxs.data(), entry.light_counts[0] * sizeof(uint32_t));
	memcpy(&entry.spot_light_idxs[0], s_l_idxs.data(), entry.light_counts[1] * sizeof(uint32_t));
	memcpy(&entry.point_light_idxs[0], p_l_idxs.data(), entry.light_counts[2] * sizeof(uint32_t));
	GH::updateBuffer(
		lightub, 
		&entry, 
		sizeof(CatcherEntry), 
		offsetof(LUBData, catcher_entries) + cidx * sizeof(CatcherEntry));
}

void Scene::updateDirSMD(size_t dl_idx, size_t sc_dl_idx, size_t smd_idx) {
	dir_lights[dl_idx].updateSMDatum(smd_idx, camera->getForward(), nullptr);
	SMEntry entry;
	entry.vp = dir_lights[dl_idx].getSMData()[smd_idx].getVP();
	entry.uv_ext_off = dir_lights[dl_idx].getSMData()[smd_idx].getUVExtOff(shadow_atlas.extent);

	GH::updateBuffer(
		lightub, 
		&entry, 
		sizeof(SMEntry), 
		offsetof(LUBData, sm_entries) + (sc_dl_idx + smd_idx) * sizeof(SMEntry));
}

void Scene::updateSpotSMD(size_t sl_idx, size_t sc_sl_idx, size_t smd_idx) {
	spot_lights[sl_idx].updateSMDatum(smd_idx, camera->getForward(), nullptr);
	SMEntry entry;
	entry.vp = spot_lights[sl_idx].getSMData()[smd_idx].getVP();
	entry.uv_ext_off = spot_lights[sl_idx].getSMData()[smd_idx].getUVExtOff(shadow_atlas.extent);

	GH::updateBuffer(
		lightub, 
		&entry, 
		sizeof(SMEntry), 
		offsetof(LUBData, sm_entries) + (sc_sl_idx + smd_idx) * sizeof(SMEntry));
}

/*
void Scene::updateSMDCascade(Light& l, size_t smd_idx, glm::vec2 depths) {
	glm::vec3 cam_points[8]; uint8_t count = 0;
	glm::mat4 vp_inv = glm::inverse(camera->getVP());
	glm::vec2 z_range;
	glm::vec4 temp = camera->getProj() * glm::vec4(0, 0, -glm::mix(camera->getNearClip(), camera->getFarClip(), depths[0]), 1);
	z_range.x = temp.z / temp.w;
	temp = camera->getProj() * glm::vec4(0, 0, -glm::mix(camera->getNearClip(), camera->getFarClip(), depths[1]), 1);
	z_range.y = temp.z / temp.w;
	for (float x = -1; x < 2; x += 2)
	for (float y = -1; y < 2; y += 2)
	// for (float z = z_range.x; z <= z_range.y; z += z_range.y - z_range.x) {
	for (float z = depths.x; z <= depths.y; z += depths.y - depths.x) {
		cam_points[count] = ProjectionBase::applyHomo(vp_inv, glm::vec3(x, y, z));
		count++;
	}

	l.updateSMDatum(smd_idx, camera->getForward(), &cam_points[0]);
	SMEntry entry;
	entry.vp = Light::smadjmat * l.getSMData()[smd_idx].getVP();
	entry.uv_ext_off = l.getSMData()[smd_idx].getUVExtOff(shadow_atlas.extent);

	GH::updateBuffer(lightub, &entry, sizeof(SMEntry), offsetof(LUBData, sm_entries) + smd_idx * sizeof(SMEntry));
}
*/

/*
void Scene::updateLUB(size_t i) {
	LUBLightEntry tempe;
	tempe.vp = Light::smadjmat * dirsclights[i].getVP();
	tempe.p = glm::vec3(0);
	tempe.c = dirsclights[i].getCol();
	GH::updateBuffer(lightub, &tempe, sizeof(LUBLightEntry), offsetof(LUBData, scdle) + sizeof(LUBLightEntry) * i);
}
*/

VkExtent2D Scene::getSAZone(VkExtent2D ext) {
	VkExtent2D res = sa_offset;
	if (sa_offset.width == 2048 - 1024) {
		if (sa_offset.height == 2048 - 1024) {
			FatalError("Out of SA space!!").raise();
		}
		else {
			sa_offset.width = 0;
			sa_offset.height += 1024;
		}
	}
	else {
		sa_offset.width += 1024;
	}
	return res;
}

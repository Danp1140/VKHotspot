#include "Scene.h"
RenderPassInfo::RenderPassInfo(
	VkRenderPass r, 
	const uint32_t nsci,
	const ImageInfo* scis,
	const ImageInfo* ms,
	const ImageInfo* d,
	std::vector<VkClearValue>&& c) : 
		renderpass(r), 
		numscis(nsci),
		clears(c) {
	extent = d ? d->extent : (ms ? ms->extent : scis[0].extent);
	// TODO: fix up createFBs to make more sense and perhaps be more flexible with attachment order
	// nsci should really be called something else
	createFBs(nsci, scis, ms, d);
}

RenderPassInfo::RenderPassInfo(VkRenderPass r, const uint32_t nsci, VkExtent2D ext, std::vector<VkClearValue>&& c, std::vector<const ImageInfo*> att, uint8_t sci_att_idx) :
	renderpass(r), numscis(nsci), extent(ext), clears(c) {
	createFBs(numscis, sci_att_idx, att);
}

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
	rendersets.push_back({p, {}, {}, {}, nullptr, pcd});
	return rendersets.size() - 1;
}

size_t RenderPassInfo::addPipeline(const PipelineInfo& p, const void* pcd, VkViewport vp, VkRect2D sc) {
	rendersets.push_back({p, {}, {}, {}, nullptr, pcd, vp, sc});
	return rendersets.size() - 1;
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
	return cbRecTaskRenderPassTemplate(renderpass, framebuffers, numscis, extent, clears.size(), clears.data());
}

Scene::Scene(float a) : 
	n_dir_lights(0), 
	n_sc_lights(0),
	n_sc_dir_lights(0),
	n_catchers(0),
	sa_offset({0, 0}) {
	camera = new Camera(glm::vec3(10, 8, 10), glm::vec3(-5, -4, -5), glm::quarter_pi<float>(), a);
	lightub.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	lightub.size = sizeof(LUBData);
	GH::createBuffer(lightub);
	GH_LOG_RESOURCE_SIZE(lightub, lightub.size)
	std::vector<LightIndex> sm_blank(3*SCENE_MAX_SC_LIGHTS, (LightIndex)-1);
	GH::updateBuffer(lightub, sm_blank.data(), sm_blank.size(), offsetof(LUBData, sm_idxs));
	
	shadow_atlas.extent = {2048, 2048};
	shadow_atlas.format = LIGHT_SHADOW_MAP_FORMAT;
	shadow_atlas.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	shadow_atlas.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	vkCreateSampler(GH::getLD(), &Light::defaultshadowsamplerci, nullptr, &shadow_atlas.sampler); 
	GH::createImage(shadow_atlas);
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

DirectionalLight* Scene::addDirectionalLight(const DirectionalLight& l, const std::vector<VkExtent2D>& sm_exts) {
	if (n_dir_lights == SCENE_MAX_DIR_LIGHTS) {
		WarningError("Maximum directional lights exceeded, not adding\n").raise();
		return nullptr;
	}
	dir_lights[n_dir_lights] = l;
	DirectionalLight& added = dir_lights[n_dir_lights];

	glm::vec4 vectors[2];
	vectors[0] = glm::vec4(added.getCol().x, added.getCol().y, added.getCol().z, 0);
	vectors[1] = glm::vec4(added.getForward().x, added.getForward().y, added.getForward().z, 0);
	GH::updateBuffer(lightub, &vectors[0], 2*sizeof(glm::vec4), offsetof(LUBData, vectors) + n_dir_lights * 2 * sizeof(glm::vec4));

	if (sm_exts.size() > 0) {
		uint32_t sm_idxs[sm_exts.size()];

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

			updateSMD(added, sm_i);

			sm_idxs[sm_i] = n_dir_lights;
			
			n_sc_lights++;
		}

		GH::updateBuffer(lightub, &sm_idxs[0], sm_exts.size()*sizeof(LightIndex), offsetof(LUBData, sm_idxs) + n_sc_dir_lights * sizeof(LightIndex));
		n_sc_dir_lights += sm_exts.size();
	}

	n_dir_lights++;
	GH::updateBuffer(lightub, &n_dir_lights, sizeof(uint32_t), offsetof(LUBData, light_counts));
	std::cout << "updateBuf w/ light count " << (int)n_dir_lights << std::endl;

	return &added;
}

std::vector<size_t> Scene::addSMPipeline(const Light& l, const PipelineInfo& p, RenderPassInfo& rpi, const void* pcd) {
	std::vector<size_t> res;
	for (const LightSMData& smd : l.getSMData()) {
		res.push_back(rpi.addPipeline(p, pcd, smd.getViewport(), smd.getScissor()));
	}
	return res;
}

void Scene::addShadowCaster(const MeshBase* m, const std::vector<uint32_t>& dl_idxs) {
	for (uint8_t i = 0; i < dl_idxs.size(); i++) {
		glm::vec3 temp = ProjectionBase::apply(m->getModelMatrix(), m->getAABB()[0]);
		for (uint8_t j = 1; j < 8; j++) {
			temp = ProjectionBase::apply(m->getModelMatrix(), glm::vec3(
						m->getAABB()[j % 2].x, 
						m->getAABB()[(uint8_t)floor(j/2) % 2].y, 
						m->getAABB()[(uint8_t)floor(j/4) % 2].z));
			for (uint8_t k = 0; k < dir_lights[dl_idxs[i]].getSMData().size(); k++) {
				dir_lights[dl_idxs[i]].getSMDatum(k).addVecToFocus(temp);
			}
		}
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

	GH::updateDS(ds, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {}, {lightub.buffer, 0, offsetof(LUBData, catcher_entries)});
	GH::updateDS(ds, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, shadow_atlas.getDII(), {});
	GH::updateDS(ds, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {}, {lightub.buffer, offsetof(LUBData, catcher_entries), SCENE_MAX_SHADOW_CATCHERS *  sizeof(CatcherEntry)});
	
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
	memcpy(&entry.dir_light_idxs[0], d_l_idxs.data(), entry.light_counts[0] * sizeof(uint32_t));
	GH::updateBuffer(lightub, &entry, sizeof(CatcherEntry), offsetof(LUBData, catcher_entries) + cidx * sizeof(CatcherEntry));
}

void Scene::updateSMD(Light& l, size_t smd_idx) {
	l.updateSMDatum(smd_idx, camera->getForward(), nullptr);
	SMEntry entry;
	entry.vp = Light::smadjmat * l.getSMData()[smd_idx].getVP();
	entry.uv_ext_off = l.getSMData()[smd_idx].getUVExtOff(shadow_atlas.extent);

	GH::updateBuffer(lightub, &entry, sizeof(SMEntry), offsetof(LUBData, sm_entries) + smd_idx * sizeof(SMEntry));
}

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
	for (float z = z_range.x; z <= z_range.y; z += z_range.y - z_range.x) {
		cam_points[count] = ProjectionBase::applyHomo(vp_inv, glm::vec3(x, y, z));
		count++;
	}

	l.updateSMDatum(smd_idx, camera->getForward(), &cam_points[0]);
	SMEntry entry;
	entry.vp = Light::smadjmat * l.getSMData()[smd_idx].getVP();
	entry.uv_ext_off = l.getSMData()[smd_idx].getUVExtOff(shadow_atlas.extent);

	GH::updateBuffer(lightub, &entry, sizeof(SMEntry), offsetof(LUBData, sm_entries) + smd_idx * sizeof(SMEntry));
}

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

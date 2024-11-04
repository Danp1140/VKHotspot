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
	rendersets.push_back({p, {}, pcd});
}

void RenderPassInfo::addMesh(const Mesh* m, size_t pidx) {
	rendersets[pidx].meshes.push_back(m);
}

std::vector<cbRecTaskTemplate> RenderPassInfo::getTasks() const {
	std::vector<cbRecTaskTemplate> tasks;
	tasks.emplace_back(getRPT());
	for (const RenderSet& r : rendersets) {
		// can't do pipeline binds out here ;-;	
		// could add a setting that puts entire pipeline & all mesh records in one 2ary cb
		// this would be more efficient for meshes that aren't swapped in and out frequently
		for (const Mesh* m : r.meshes) {
			tasks.emplace_back(
					[d = m->getDrawData(
						renderpass, 
						r.pipeline, 
						{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePCData)}, 
						r.pcdata), 
					f = framebuffers] 
					(uint8_t scii, VkCommandBuffer& c) {
				Mesh::recordDraw(f[scii], d, c);
			});
		}
	}
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
	if (scis) {
		for (uint8_t scii = 0; scii < nsci; scii++) {
			attachments[0] = scis[scii].view;
			vkCreateFramebuffer(GH::getLD(), &framebufferci, nullptr, &framebuffers[scii]);
		}
	}
}

// could p be inline/constexpr
cbRecTaskRenderPassTemplate RenderPassInfo::getRPT() const {
	return cbRecTaskRenderPassTemplate(renderpass, framebuffers, extent, clears.size(), clears.data());
}

Scene::Scene(float a) {
	camera = new Camera(glm::vec3(3), glm::vec3(-3), glm::quarter_pi<float>(), a);
	// GH::createDS(ds);
	lightub = {};
	/*
	VkDescriptorSetLayoutBinding bindings[1] = {{
		0,

	}};
	p.descsetlayoutci = {
		VK_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		0, nullptr 
	}
	*/
}

Scene::~Scene() {
	if (lightub.buffer != VK_NULL_HANDLE) GH::destroyBuffer(lightub);
	// GH::destroyDS(ds);
	delete camera;
	for (RenderPassInfo& r : renderpasses) r.destroy();
}

std::vector<cbRecTaskTemplate> Scene::getDrawTasks() {
	std::vector<cbRecTaskTemplate> result;
	// TODO: bind scene ds here!
	std::vector<cbRecTaskTemplate> temp;
	for (const RenderPassInfo& r : renderpasses) {
		temp = r.getTasks();
		result.insert(result.end(), temp.begin(), temp.end());
	}
	return result;
}

void Scene::addRenderPass(const RenderPassInfo& r) {
	renderpasses.push_back(r);
}


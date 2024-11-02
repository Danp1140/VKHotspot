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
	for (PipelineInfo& p : pipelines) GH::destroyPipeline(p);
	if (framebuffers) {
		for (uint8_t scii = 0; scii < numscis; scii++) vkDestroyFramebuffer(GH::getLD(), framebuffers[scii], nullptr);
		delete[] framebuffers;
	}
	if (renderpass != VK_NULL_HANDLE) GH::destroyRenderPass(renderpass);
}

void RenderPassInfo::addPipeline(const PipelineInfo& p) {
	pipelines.push_back(p);
}

void RenderPassInfo::addMesh(const Mesh* m) {
	meshes.push_back(m);
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

Scene::Scene() {
	camera = new Camera;
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
	// would like a system of defaults, but also one that allows custom rps & ops
	std::vector<cbRecTaskTemplate> result;
	for (const RenderPassInfo& r : renderpasses) {
		// TODO: move contents of this for loop to function in RenderPassInfo
		result.emplace_back(cbRecTaskRenderPassTemplate(
			r.getRenderPass(),
			r.getFramebuffers(),
			r.getExtent(),
			r.getClears().size(),
			r.getClears().data()));
		for (const Mesh* m : r.getMeshes()) {
			result.emplace_back(cbRecFuncTemplate(
				r.getRenderPass(),
				&m->getGraphicsPipeline(),
				ds, m->getDS(),
				m->getVertexBuffer().buffer, m->getIndexBuffer().buffer,
				m->getIndexBuffer().size / sizeof(MeshIndex),
				Mesh::recordDraw,
				r.getFramebuffers()));
		}
	}
	return result;
}

void Scene::addRenderPass(const RenderPassInfo& r) {
	renderpasses.push_back(r);
}


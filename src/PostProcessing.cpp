#include "PostProcessing.h"

PPStep::PPStep(const WindowInfo* w, const char* shader_fpp) : window(w), fb(nullptr) {
	src.extent = window->getSCExtent();
	src.format = GH_SWAPCHAIN_IMAGE_FORMAT;
	src.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	src.layout = VK_IMAGE_LAYOUT_GENERAL; // TODO see if we can specialize/transition
	src.sampler = GH::getNearestSampler();
	GH::createImage(src);
	depth_res.extent = window->getSCExtent();
	depth_res.format = GH_DEPTH_BUFFER_IMAGE_FORMAT;
	depth_res.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	depth_res.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // TODO see if we can specialize/transition
	depth_res.sampler = GH::getNearestSampler();
	GH::createImage(depth_res);

	createRenderPass();
	createPipeline();
}

PPStep::~PPStep() {
	GH::destroyPipeline(pipeline);
	GH::destroyRenderPass(rp);
	if (fb) {
		for (uint8_t scii = 0; scii < window->getNumSCIs(); scii++) vkDestroyFramebuffer(GH::getLD(), fb[scii], nullptr);
		delete fb;
	}
	GH::destroyImage(depth_res);
	GH::destroyImage(src);
}

void PPStep::recordCopy(uint8_t scii, VkCommandBuffer& c, const ImageInfo* src, const ImageInfo& dst) {
	VkCommandBufferInheritanceInfo cbinherinfo {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		nullptr,
		VK_NULL_HANDLE, 0,
		VK_NULL_HANDLE,
		VK_FALSE, 0, 0
	};
	VkCommandBufferBeginInfo cbbi {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		nullptr,
		0,
		&cbinherinfo
	};
	vkBeginCommandBuffer(c, &cbbi);
	const VkImageSubresourceLayers subr {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
	const VkImageCopy cpy {
		subr, {0, 0, 0},
		subr, {0, 0, 0},
		{dst.extent.width, dst.extent.height, 1}
	};
	// vkCmdCopyImage(c, src[scii].image, src[scii].layout, dst.image, dst.layout, 1, &cpy);
	vkCmdCopyImage(c, src[scii].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.image, dst.layout, 1, &cpy);
	vkEndCommandBuffer(c);
}

void PPStep::recordDepthResolve(uint8_t scii, VkCommandBuffer& c, const ImageInfo& ms_db, const ImageInfo& db) {
	VkCommandBufferInheritanceInfo cbinherinfo {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		nullptr,
		VK_NULL_HANDLE, 0,
		VK_NULL_HANDLE,
		VK_FALSE, 0, 0
	};
	VkCommandBufferBeginInfo cbbi {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		nullptr,
		0,
		&cbinherinfo
	};
	vkBeginCommandBuffer(c, &cbbi);
	const VkImageSubresourceLayers subr {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1};
	const VkImageResolve res {
		subr, {0, 0, 0},
		subr, {0, 0, 0},
		{db.extent.width, db.extent.height, 1}
	};
	vkCmdResolveImage(c, ms_db.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, db.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, &res); 
	vkEndCommandBuffer(c);
}

void PPStep::recordDraw(uint8_t scii, VkCommandBuffer& c, const PPRenderSet& rs) {
	VkCommandBufferInheritanceInfo cbinherinfo {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		nullptr,
		rs.rp, 0,
		rs.fb[scii],
		VK_FALSE, 0, 0
	};
	VkCommandBufferBeginInfo cbbi {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		nullptr,
		VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
		&cbinherinfo
	};
	vkBeginCommandBuffer(c, &cbbi);
	vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_GRAPHICS, rs.pipeline.pipeline);
	vkCmdPushConstants(c, rs.pipeline.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(temp_pc_dat), rs.pcdata);
	vkCmdBindDescriptorSets(
		c,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		rs.pipeline.layout,
		0, 1, &rs.ds,
		0, nullptr);
	vkCmdDraw(c, 6, 1, 0, 0);
	vkEndCommandBuffer(c);
}

void PPStep::createRenderPass() {
	VkAttachmentDescription attachdescs[1] {{
			0, 
			GH_SWAPCHAIN_IMAGE_FORMAT,
			VK_SAMPLE_COUNT_1_BIT, // TODO: do we just take the MSAA samples???
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	}};
	VkAttachmentReference attachrefs[1] {
		{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
	};
	GH::createRenderPass(rp, 1, &attachdescs[0], &attachrefs[0], nullptr, nullptr);
	clear = {0, 0.3, 0, 1};
	// RenderPassInfo rpi(rp, window->getNumSCIs(), window->getSCImages(), nullptr, nullptr, {clear});

	fb = new VkFramebuffer[window->getNumSCIs()];
	for (uint8_t scii = 0; scii < window->getNumSCIs(); scii++) {
		VkFramebufferCreateInfo framebufferci {
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			nullptr,
			0,
			rp,
			1, &window->getSCImages()[scii].view,
			window->getSCExtent().width, window->getSCExtent().height, 1
		};
		vkCreateFramebuffer(GH::getLD(), &framebufferci, nullptr, &fb[scii]);
	}
}

void PPStep::createPipeline() {
	pipeline.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pipeline.shaderfilepathprefix = "postproc";
	VkDescriptorSetLayoutBinding bindings[3] {{
		0, // color
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	}, {
		1, // depth
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	}, {
		2, // shadow map
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	}};
	pipeline.descsetlayoutci = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		3, &bindings[0]
	};
	pipeline.extent = window->getSCExtent();
	pipeline.renderpass = rp;
	pipeline.pushconstantrange = {VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(temp_pc_dat)};
	GH::createPipeline(pipeline);

	VkDescriptorImageInfo lyingdii = depth_res.getDII();
	lyingdii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	GH::createDS(pipeline, ds);
	GH::updateDS(ds, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, src.getDII(), {});
	GH::updateDS(ds, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, lyingdii, {});
}


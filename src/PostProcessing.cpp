#include "PostProcessing.h"

PPStep::PPStep(const WindowInfo* w, const char* shader_fpp) : window(w), fb(nullptr) {
	dst.extent = window->getSCExtent();
	dst.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	dst.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	dst.layout = VK_IMAGE_LAYOUT_GENERAL; // TODO see if we can specialize/transition
	GH::createImage(dst);

	createRenderPass();
	createProcPipeline(shader_fpp); 
	createDrawPipeline();
}

PPStep::~PPStep() {
	GH::destroyPipeline(pipeline);
	if (fb) delete fb;
}

void PPStep::recordProc(uint8_t scii, VkCommandBuffer& c, const PPRenderSet& rs) {
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
		VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
		&cbinherinfo
	};
	vkBeginCommandBuffer(c, &cbbi);
	vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_COMPUTE, rs.pipeline.pipeline);
	vkCmdBindDescriptorSets(
		c,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		rs.pipeline.layout,
		0, 1, &rs.ds,
		0, nullptr);
	vkCmdPushConstants(
		c, 
		rs.pipeline.layout, 
		rs.pipeline.pushconstantrange.stageFlags, 
		rs.pipeline.pushconstantrange.offset, 
		rs.pipeline.pushconstantrange.size, 
		rs.pcdata);
	vkCmdDispatch(c, rs.pipeline.extent.width, rs.pipeline.extent.height, 1);
	vkEndCommandBuffer(c);
}

void PPStep::recordDraw(uint8_t scii, VkCommandBuffer& c, PipelineInfo p, VkDescriptorSet ds, VkRenderPass rp, const VkFramebuffer* fb) {
	VkCommandBufferInheritanceInfo cbinherinfo {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		nullptr,
		rp, 0,
		fb[scii],
		VK_FALSE, 0, 0
	};
	VkCommandBufferBeginInfo cbbi {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		nullptr,
		VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
		&cbinherinfo
	};
	vkBeginCommandBuffer(c, &cbbi);
	vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_GRAPHICS, p.pipeline);
	vkCmdBindDescriptorSets(
		c,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		p.layout,
		0, 1, &ds,
		0, nullptr);
	vkCmdDrawIndexed(c, 6, 1, 0, 0, 0);
	vkEndCommandBuffer(c);
}

void PPStep::createRenderPass() {
	VkAttachmentDescription attachdescs[3] {{
			0, 
			GH_SWAPCHAIN_IMAGE_FORMAT,
			VK_SAMPLE_COUNT_1_BIT, // TODO: do we just take the MSAA samples???
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
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

void PPStep::createProcPipeline(const char* fpp) {
	pipeline.stages = VK_SHADER_STAGE_COMPUTE_BIT;
	pipeline.shaderfilepathprefix = fpp;
	VkDescriptorSetLayoutBinding bindings[4] {{
		0, // color
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		VK_SHADER_STAGE_COMPUTE_BIT,
		nullptr
	}, {
		1, // depth
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		VK_SHADER_STAGE_COMPUTE_BIT,
		nullptr
	}, {
		2, // temp shadowmap
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		VK_SHADER_STAGE_COMPUTE_BIT,
		nullptr
	}, {
		3, // result
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		1,
		VK_SHADER_STAGE_COMPUTE_BIT,
		nullptr
	}};
	pipeline.descsetlayoutci = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		4, &bindings[0]
	};
	pipeline.pushconstantrange = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(temp_pc_dat)};
	pipeline.extent = window->getSCExtent();
	GH::createPipeline(pipeline);

	GH::createDS(pipeline, ds);
	GH::updateDS(ds, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, window->getSCImages()[0].getDII(), {}); // TODO: one ds per sci
	GH::updateDS(ds, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, window->getDepthBuffer()->getDII(), {}); 
	GH::updateDS(ds, 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, dst.getDII(), {});
}

void PPStep::createDrawPipeline() {
	draw_pipeline.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	draw_pipeline.shaderfilepathprefix = "postproc";
	VkDescriptorSetLayoutBinding bindings[1] {{
		0, // color
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	}};
	draw_pipeline.descsetlayoutci = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1, &bindings[0]
	};
	draw_pipeline.extent = window->getSCExtent();
	GH::createPipeline(draw_pipeline);

	GH::createDS(draw_pipeline, draw_ds);
	GH::updateDS(ds, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, dst.getDII(), {});
}


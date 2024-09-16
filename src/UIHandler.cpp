#include "UIHandler.h"

UIHandler::UIHandler(VkExtent2D extent) :
	colorclear({0.4, 0.1, 0.1, 1}) {
	VkAttachmentDescription colordesc {
		0,
		GH_SWAPCHAIN_IMAGE_FORMAT,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};
	VkAttachmentReference colorref {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	GH::createRenderPass(renderpass, 1, &colordesc, &colorref, nullptr);

	graphicspipeline.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	graphicspipeline.shaderfilepathprefix = "UI";
	graphicspipeline.renderpass = renderpass;
	graphicspipeline.extent = extent; 
	graphicspipeline.cullmode = VK_CULL_MODE_NONE;
	graphicspipeline.pushconstantrange = {
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof(UIPushConstantData)
	};
	VkDescriptorSetLayoutBinding dslbindings[1] {{
		0,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	}};
	graphicspipeline.descsetlayoutci = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1, &dslbindings[0]
	};
	GH::createPipeline(graphicspipeline);

	// v quick & dirty
	VkDescriptorSet ds;
	GH::createDS(graphicspipeline, ds);
	UIComponent::setDefaultDS(ds);

	UIText::setTexLoadFunc([] (UIText* t, unorm* d) {
		ImageInfo i = uiToGHImageInfo(t->getTex());
		if (i.image != VK_NULL_HANDLE) {
			vkQueueWaitIdle(GH::getGenericQueue());
			GH::destroyImage(i);
		}
		GH::createImage(i);
		GH::updateImage(i, d);
		GH::updateDS(t->getDS(), 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, i.getDII(), {});
		
		t->setTex(ghToUIImageInfo(i));
	});
	UIText::setTexDestroyFunc([] (UIText* t) {
		ImageInfo i = uiToGHImageInfo(t->getTex());
		GH::destroyImage(i);
		t->setTex(ghToUIImageInfo(i));
	});

	// should just set draw func of our root object
	UIComponent::setDefaultDrawFunc([] (const UIComponent* const c, const VkCommandBuffer& cb) {
		vkCmdBindPipeline(
			cb, 
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			c->getGraphicsPipeline().pipeline);
		vkCmdBindDescriptorSets(
			cb,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			c->getGraphicsPipeline().layout,
			0, 1, &c->getDS(), 
			0, nullptr);
		vkCmdPushConstants(
			cb,
			c->getGraphicsPipeline().layout,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0, sizeof(UIPushConstantData),
			&c->getPCData());
		vkCmdDraw(cb, 6, 1, 0, 0);
	});

	root = UIText(L"please work", UICoord(extent.width / 2, extent.height / 2));
	// root = UIText(L"please work", UICoord(100));

	root.setGraphicsPipeline(ghToUIPipelineInfo(graphicspipeline));

	cbbegininfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		nullptr, 
		VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
		&cbinherinfo // watch out when you make a copy constructor!!
	};
	cbinherinfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		nullptr,
		renderpass, // watch out if we ever need a setRenderPass function
		0, // hard-coded subpass :/
		VK_NULL_HANDLE,
		VK_FALSE, 0, 0
	};
}

UIHandler::~UIHandler() {
	vkQueueWaitIdle(GH::getGenericQueue());
	GH::destroyPipeline(graphicspipeline);
	GH::destroyRenderPass(renderpass);
}

void UIHandler::draw(VkCommandBuffer& cb, const VkFramebuffer& f) {
	cbinherinfo.framebuffer = f;
	vkBeginCommandBuffer(cb, &cbbegininfo);
	root.draw(cb);
	vkEndCommandBuffer(cb);
}
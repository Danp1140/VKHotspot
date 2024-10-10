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

	VkDescriptorSet ds;
	GH::createDS(graphicspipeline, ds);
	UIComponent::setDefaultDS(ds);
	ImageInfo i;
	// little bodge to populate default fields like format
	i = uiToGHImageInfo(ghToUIImageInfo(i));
	i.extent = {1, 1};
	GH::createImage(i);
	UIComponent::setNoTex(ghToUIImageInfo(i));
	GH::updateDS(ds, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, i.getDII(), {});

	UIImage::setTexLoadFunc([] (UIImage* t, void* d) {
		if (t->getDS() == VK_NULL_HANDLE || t->getDS() == UIComponent::getDefaultDS()) return;
		ImageInfo i = uiToGHImageInfo(t->getTex());
		if (i.image != VK_NULL_HANDLE) {
			vkQueueWaitIdle(GH::getGenericQueue());
			GH::destroyImage(i);
		}
		GH::createImage(i);
		GH::updateImage(i, d);
		GH::updateDS(t->getDS(), 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, i.getDII(), {});
		
		t->setTex(ghToUIImageInfo(i));

#ifdef VERBOSE_UI_CALLBACKS
		std::cout << "created image " << i.image << " with view " << i.view << std::endl;
#endif
	});
	UIImage::setTexDestroyFunc([] (UIImage* t) {
		ImageInfo i = uiToGHImageInfo(t->getTex());
#ifdef VERBOSE_UI_CALLBACKS 
		std::cout << "destroyed image " << i.image << " with view " << i.view << std::endl;
#endif
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

	root = UIContainer();
	GH::createDS(graphicspipeline, ds);
	UIText temp = UIText(L"please work", UICoord(extent.width / 2, extent.height / 2));
	temp.setDS(ds);
	root.addChild(temp);

	GH::createDS(graphicspipeline, ds);
	temp = UIText(L"please work too ;-;", UICoord(extent.width / 4, extent.height / 4));
	temp.setDS(ds);
	root.addChild(temp);

	GH::createDS(graphicspipeline, ds);
	UIImage itemp = UIImage(UICoord(extent.width * 3 / 4, extent.height * 3 / 4));
	itemp.setDS(ds);
	UIImageInfo infotemp;
	infotemp.format = VK_FORMAT_R8G8B8A8_SRGB;
	infotemp.extent = {1024, 1024};
	/*
	ImageInfo ghinfotemp = uiToGHImageInfo(infotemp);
	GH::createImage(ghinfotemp);
	*/
	itemp.setTex(infotemp);
	
	uint8_t* colors = new uint8_t[1024 * 1024 * 4];
	for (uint32_t i = 0; i < 1024; i++) {
		for (uint32_t j = 0; j < 1024; j++) {
			colors[(i * 1024 + j) * 4] = (uint8_t)((float)i / 1024. * 255.);
			colors[(i * 1024 + j) * 4 + 1] = (uint8_t)((float)j / 1024. * 255.);
			colors[(i * 1024 + j) * 4 + 2] = (uint8_t)255;
			colors[(i * 1024 + j) * 4 + 3] = (uint8_t)255;
		}
	}
	// GH::updateImage(ghinfotemp, colors);
	UIImage::texLoadFunc(&itemp, colors);
	delete[] colors;
	itemp.setExt(UICoord(1024, 1024));
	root.addChild(itemp);

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
	ImageInfo temp = uiToGHImageInfo(UIComponent::getNoTex());
	GH::destroyImage(temp);
	GH::destroyPipeline(graphicspipeline);
	GH::destroyRenderPass(renderpass);
}

void UIHandler::draw(VkCommandBuffer& cb, const VkFramebuffer& f) {
	cbinherinfo.framebuffer = f;
	vkBeginCommandBuffer(cb, &cbbegininfo);
	root.draw(cb);
	vkEndCommandBuffer(cb);
}

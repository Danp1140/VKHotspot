#include "UIHandler.h"

UIHandler::UIHandler(const PipelineInfo& p, VkExtent2D extent) {
	UIComponent::setDefaultGraphicsPipeline(ghToUIPipelineInfo(p));

	VkDescriptorSet ds;
	GH::createDS(p, ds);
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
}

UIHandler::~UIHandler() {
	ImageInfo temp = uiToGHImageInfo(UIComponent::getNoTex());
	GH::destroyImage(temp);
}

void UIHandler::setTex(UIImage& i, const ImageInfo& ii, const PipelineInfo& p) {
	VkDescriptorSet dstemp;
	GH::createDS(p, dstemp);
	GH::updateDS(dstemp, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ii.getDII(), {});
	i.setTex(ghToUIImageInfo(ii));
	i.setDS(dstemp);
}

void UIHandler::recordDraw(VkFramebuffer f, VkRenderPass rp, VkCommandBuffer& cb) const {
	VkCommandBufferInheritanceInfo cbinherinfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		nullptr,
		rp,
		0, // hard-coded subpass :/
		f,
		VK_FALSE, 0, 0
	};
	VkCommandBufferBeginInfo cbbegininfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		nullptr, 
		VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
		&cbinherinfo
	};

	vkBeginCommandBuffer(cb, &cbbegininfo);
	root.draw(cb);
	vkEndCommandBuffer(cb);
}

#include "UIHandler.h"

VkSampler UIHandler::textsampler = VK_NULL_HANDLE;

UIHandler::UIHandler(const PipelineInfo& p, VkExtent2D extent) {
	UIComponent::setDefaultGraphicsPipeline(ghToUIPipelineInfo(p));

	// --- Text Sampler Setup ---
	VkSamplerCreateInfo ci {
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		nullptr,
		0,
		VK_FILTER_LINEAR, VK_FILTER_LINEAR,
		VK_SAMPLER_MIPMAP_MODE_NEAREST,
		VK_SAMPLER_ADDRESS_MODE_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_REPEAT,
		0, 
		VK_FALSE, 0, 
		VK_FALSE, VK_COMPARE_OP_NEVER,
		0, 1,
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
		VK_FALSE
	};
	vkCreateSampler(GH::getLD(), &ci, nullptr, &textsampler);

	// --- Blank Tex & DS Setup ---
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
		ImageInfo i = uiToGHImageInfo(t->getTex());
		// if given no data, set to static blank image
		if (!d) i = uiToGHImageInfo(UIComponent::getNoTex());
		// otherwise, make a new one and fill it
		else {
			GH::createImage(i);
			GH::updateImage(i, d);
		}
		// if this a DS we can't or shouldn't touch, make a new one to update to
		if (t->getDS() == VK_NULL_HANDLE || t->getDS() == UIComponent::getDefaultDS()) {
			PipelineInfo pitemp {
				t->getGraphicsPipeline().layout,
				t->getGraphicsPipeline().pipeline,
				t->getGraphicsPipeline().dsl
			};
			VkDescriptorSet temp;
			// TODO: consider making some copy functions that take just relevant struct members as args...
			// also, be careful not to create too many DS's; we don't ever free them, only update them...
			// if we want to ensure that, we should destroy the default DS completely...
			GH::createDS(pitemp, temp);
			t->setDS(temp);
		}
		vkQueueWaitIdle(GH::getGenericQueue());
		GH::updateDS(t->getDS(), 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, i.getDII(), {});
#ifdef VERBOSE_UI_CALLBACKS
		std::cout << "created image " << i.image << " with view " << i.view << std::endl;
#endif
		t->setTex(ghToUIImageInfo(i));
	});
	UIImage::setTexDestroyFunc([] (UIImage* t) {
		ImageInfo i = uiToGHImageInfo(t->getTex());
#ifdef VERBOSE_UI_CALLBACKS 
		std::cout << "destroyed image " << i.image << " with view " << i.view << std::endl;
#endif
		// should we wait for queue idle???
		GH::destroyImage(i);
		// is this second set really warranted?
		// t->setTex(ghToUIImageInfo(i));
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
	root.setBGCol({0, 0, 0, 0}); // TODO: see if we can actually just make it not draw at all lol
	root.setExt(UICoord(extent.width, extent.height));
}

UIHandler::~UIHandler() {
	ImageInfo temp = uiToGHImageInfo(UIComponent::getNoTex());
	GH::destroyImage(temp);
	vkDestroySampler(GH::getLD(), textsampler, nullptr);
}

void UIHandler::setTex(UIImage& i, const ImageInfo& ii, const PipelineInfo& p) {
	// TODO: see if we can make this reusable in texture callback
	VkDescriptorSet dstemp = i.getDS();
	if (dstemp == VK_NULL_HANDLE || dstemp == UIComponent::getDefaultDS()) {
		GH::createDS(p, dstemp);
		i.setDS(dstemp);
	}
	GH::updateDS(dstemp, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ii.getDII(), {});
	i.setTex(ghToUIImageInfo(ii));
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

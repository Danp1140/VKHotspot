#include "GraphicsHandler.h"
#include "UI.h"

// #define VERBOSE_UI_CALLBACKS

class UIHandler {
public:
	UIHandler() = delete;
	UIHandler(const UIHandler& lvalue) = delete;
	UIHandler(UIHandler&& rvalue) = delete;
	UIHandler(const PipelineInfo& p, VkExtent2D extent);
	~UIHandler();

	/* 
	 * another hack to get polymorphic functions to work
	 * (e.g., text needs to regen tex on ds set)
	 */
	// TODO: should this be a reference of some sort???
	template <class T>
	void addComponent(T c) {
		// TODO: is all this ds stuff needed for non-tex pipelines?
		// WHY ARENT DESCSETS MANAGED BY THE COMPONENETS????????
		VkDescriptorSet dstemp;
		PipelineInfo pitemp;
		pitemp.pipeline = c.getGraphicsPipeline().pipeline;
		pitemp.layout = c.getGraphicsPipeline().layout;
		pitemp.dsl = c.getGraphicsPipeline().dsl;
		GH::createDS(pitemp, dstemp);
		c.setDS(dstemp);
		root.addChild(c);
	}
	void setTex(UIImage& i, const ImageInfo& ii, const PipelineInfo& p);

	void recordDraw(VkFramebuffer f, VkRenderPass rp, VkCommandBuffer& cb) const;

	static constexpr UIPipelineInfo ghToUIPipelineInfo(const PipelineInfo& p) {
		return (UIPipelineInfo){
			p.layout,
			p.pipeline,
			p.dsl,
			p.descsetlayoutci,
			p.pushconstantrange,
			p.specinfo
		};
	}
	static constexpr UIImageInfo ghToUIImageInfo(const ImageInfo& i) {
		return (UIImageInfo) {
			i.image,
			i.memory,
			i.view,
			i.extent,
			i.layout
		};
	}
	// can't be constexpr because it needs to grab the GH sampler
	static ImageInfo uiToGHImageInfo(const UIImageInfo& i) {
		return (ImageInfo) {
			i.image,
			i.memory,
			i.view,
			i.extent,
			i.format,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // for now assuming dynamic text
			VK_IMAGE_LAYOUT_GENERAL,
			GH::getNearestSampler(),
			// below should probably depend on static vs dynamic tex
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		};
	}

// would like to have option for static vs dynamic textures...

private:
	VkCommandBufferBeginInfo cbbegininfo;
	VkCommandBufferInheritanceInfo cbinherinfo;

	UIContainer root;
};

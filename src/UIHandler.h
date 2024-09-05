#include "GraphicsHandler.h"
#include "UI.h"

class UIHandler {
public:
	UIHandler() = delete;
	UIHandler(const UIHandler& lvalue) = delete;
	UIHandler(UIHandler&& rvalue) = delete;
	UIHandler(VkExtent2D extent);
	~UIHandler();

	// later make this a container component
	UIText root;

	void draw(VkCommandBuffer& cb, const VkFramebuffer& f) ;

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
	static constexpr ImageInfo uiToGHImageInfo(const UIImageInfo& i) {
		return (ImageInfo) {
			i.image,
			i.memory,
			i.view,
			i.extent,
			VK_FORMAT_R8_UNORM, // should probably be def'd in UI.h
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // for now assuming dynamic text
			VK_IMAGE_LAYOUT_GENERAL
		};
	}

	const VkRenderPass& getRenderPass() const {return renderpass;}
	const VkClearValue& getColorClear() const {return colorclear;}

// would like to have option for static vs dynamic textures...

private:
	VkRenderPass renderpass;
	PipelineInfo graphicspipeline;
	VkCommandBufferBeginInfo cbbegininfo;
	VkCommandBufferInheritanceInfo cbinherinfo;
	const VkClearValue colorclear;
};

#include "UIHandler.h"

VkRenderPass UIHandler::renderpass = VK_NULL_HANDLE;

void UIHandler::init() {
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
}

void UIHandler::terminate() {
	vkQueueWaitIdle(GH::getGenericQueue());
	GH::destroyRenderPass(renderpass);
}

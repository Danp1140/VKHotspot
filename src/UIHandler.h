#include "GraphicsHandler.h"
#include "UI.h"

class UIHandler {
public:
	UIHandler() = delete;
	~UIHandler() = delete;

	static void init();
	static void terminate();

	static const VkRenderPass& getRenderPass() {return renderpass;}

private:
	static VkRenderPass renderpass;
};

#include "UIHandler.h"
#include "Scene.h"

void createScene(Scene& s, const WindowInfo& w, const Mesh& m) {
	VkRenderPass r;
	VkAttachmentDescription attachdescs[2] {{
			0, 
			GH_SWAPCHAIN_IMAGE_FORMAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		}, {
			0, 
			GH_DEPTH_BUFFER_IMAGE_FORMAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
	}};
	VkAttachmentReference attachrefs[2] {
		{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
		{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}
	};
	GH::createRenderPass(r, 2, &attachdescs[0], &attachrefs[0], &attachrefs[1]);

	RenderPassInfo rpi(r, w.getNumSCIs(), w.getSCImages(), w.getDepthBuffer(), {{0, 0.4, 0.1, 1}, {1, 0}});

	PipelineInfo p;
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "default";
	p.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePCData)};
	p.vertexinputstateci = Mesh::getVISCI(VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL);
	p.depthtest = true;
	p.extent = w.getSCExtent();
	p.renderpass = r;
	p.cullmode = VK_CULL_MODE_NONE; // temp to troubleshoot
	GH::createPipeline(p);
	rpi.addPipeline(p, &s.getCamera()->getVP());
	
	rpi.addMesh(&m, 0);

	s.addRenderPass(rpi);
}

int main() {
	GH graphicshandler = GH();
	WindowInfo w;
	UIHandler ui(w.getSCExtent());
	w.setPresentationRP(ui.getRenderPass());

	/*
	w.addTask(cbRecTaskTemplate(cbRecTaskRenderPassTemplate(
		ui.getRenderPass(),
		w.getPresentationFBs(),
		w.getSCImages()[0].extent,
		1, &ui.getColorClear())));
	// TODO: there's gotta be a better way to do this, ui draws shouldn't be this special
	w.addTask(cbRecTaskTemplate(cbRecFuncTemplate(
		VK_NULL_HANDLE,
		nullptr,
		VK_NULL_HANDLE, VK_NULL_HANDLE,
		VK_NULL_HANDLE, VK_NULL_HANDLE,
		[&ui, &w] (cbRecData d, VkCommandBuffer& cb) { ui.draw(cb, w.getCurrentPresentationFB()); },
		w.getPresentationFBs())));
	ui.addComponent(UIText(L"text from main", UICoord(1000, 1000)));
	*/
	
	Scene s((float)w.getSCExtent().width / (float)w.getSCExtent().height);
	Mesh m("resources/models/objs/cube.obj");
	createScene(s, w, m);
	w.addTasks(s.getDrawTasks());

	bool xpressed = false;
	SDL_Event eventtemp;
	while (!xpressed) {
		w.frameCallback();
		// w2.frameCallback();

		while (SDL_PollEvent(&eventtemp)) {
			if (eventtemp.type == SDL_EVENT_QUIT
				|| eventtemp.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
				xpressed = true;
			}
		}
	}
	vkQueueWaitIdle(GH::getGenericQueue());

	return 0;
}

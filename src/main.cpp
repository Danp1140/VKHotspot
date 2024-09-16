#include "UIHandler.h"

int main() {
	GH graphicshandler = GH();
	WindowInfo w;
	UIHandler ui(w.getSCExtent());
	w.setPresentationRP(ui.getRenderPass());
	/*
	WindowInfo w2;
	UIHandler ui2(w2.getSCExtent());
	w2.setPresentationRP(ui2.getRenderPass());
	*/

	VkExtent2D tempext[w.getNumSCIs()];
	for (uint8_t scii = 0; scii < w.getNumSCIs(); scii++) tempext[scii] = w.getSCImages()[scii].extent;
	w.addTask(cbRecTaskTemplate(cbRecTaskRenderPassTemplate(
		ui.getRenderPass(),
		w.getPresentationFBs(),
		tempext,
		1, &ui.getColorClear(),
		w.getNumSCIs())));
	w.addTask(cbRecFunc([&ui, &w] (VkCommandBuffer& cb) {
		ui.draw(cb, w.getCurrentPresentationFB());
	}));

	/*
	w2.addTask(cbRecTaskTemplate(cbRecTaskRenderPassTemplate(
		ui2.getRenderPass(),
		w2.getPresentationFBs(),
		tempext,
		1, &ui2.getColorClear(),
		w2.getNumSCIs())));
	w2.addTask(cbRecFunc([&ui2, &w2] (VkCommandBuffer& cb) {
		ui2.draw(cb, w2.getCurrentPresentationFB());
	}));
	*/

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

	return 0;
}

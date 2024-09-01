#include "UIHandler.h"

int main() {
	GH graphicshandler = GH();
	UIHandler::init();
	WindowInfo w(UIHandler::getRenderPass());

	UIText testtex("please work", glm::vec2(100, 100));

	bool xpressed = false;
	SDL_Event eventtemp;
	while (!xpressed) {
		w.frameCallback();

		while (SDL_PollEvent(&eventtemp)) {
			if (eventtemp.type == SDL_EVENT_QUIT
				|| eventtemp.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
				xpressed = true;
			}
		}
	}


	UIHandler::terminate();
	return 1;
}

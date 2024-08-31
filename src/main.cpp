#include "GraphicsHandler.h"

int main() {
	GH graphicshandler = GH();
	WindowInfo w = WindowInfo();
	WindowInfo w2 = WindowInfo();

	bool xpressed = false;
	SDL_Event eventtemp;
	while (!xpressed) {
		w.frameCallback();
		w2.frameCallback();

		while (SDL_PollEvent(&eventtemp)) {
			if (eventtemp.type == SDL_EVENT_QUIT
				|| eventtemp.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
				xpressed = true;
			}
		}
	}

	return 1;
}

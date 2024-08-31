#include "GraphicsHandler.h"

int main() {
	GH graphicshandler = GH();
	WindowInfo w = WindowInfo();

	while (!w.shouldClose()) {
		w.frameCallback();
	}

	return 1;
}

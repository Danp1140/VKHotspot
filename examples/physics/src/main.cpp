#include "Scene.h"

int main() {
	GH gh;
	WindowInfo w;

	while (w.frameCallback()) {}

	vkQueueWaitIdle(GH::getGenericQueue());

	return 0;
}

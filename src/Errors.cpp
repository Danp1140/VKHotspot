#include "Errors.h"

/*
 * Error
 */

Error::Error(const std::string& m) {
	message = std::string(m);
}

Error::Error(std::string&& m) {
	message = std::move(m);
}

/*
 * FatalError
 */

void FatalError::raise() {
	std::cerr << "Fatal Error: " << message << std::endl;
	exit(1);
}

void FatalError::vkCatch(VkResult r) {
	if (r != VK_SUCCESS) {
		message += "VkResult: "; 
		message += string_VkResult(r);
		raise();
	}
}

void FatalError::sdlCatch(SDL_bool r) {
	if (!r) {
		message += "SDL_GetError(): ";
		message += SDL_GetError();
		raise();
	}
}

/*
 * WarningError
 */

void WarningError::raise() {
	std::cerr << "Warning Error: " << message << std::endl;
}

#include "Errors.h"

/*
 * FatalError
 */

FatalError::FatalError(const std::string& m) {
	message = std::string(m);
}

FatalError::FatalError(std::string&& m) {
	message = std::move(m);
}

void FatalError::raise() {
	std::cerr << "Fatal Error: " << message << std::endl;
	exit(1);
}

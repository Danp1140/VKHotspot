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

/*
 * WarningError
 */

void WarningError::raise() {
	std::cerr << "Warning Error: " << message << std::endl;
}

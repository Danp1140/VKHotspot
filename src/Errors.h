#include <iostream>

#include <vk_enum_string_helper.h>
#include <SDL3/SDL.h>

class Error {
public:
	virtual void raise() = 0;

protected:
	Error() = default;
	~Error() = default;
	Error(const std::string& m);
	Error(std::string&& m);

	std::string message;
};

class FatalError : public Error {
public:
	FatalError(const std::string& m) : Error(m) {}
	FatalError(std::string&& m) : Error(m) {}
	~FatalError() = default;

	void raise();
	// used to raise a FatalError if a vk func fails
	// for efficiency, should only be used on infrequent (non-per-frame) vk function calls
	void vkCatch(VkResult r);
	// vkCatch but for SDL ops. reads SDL_GetError()
	void sdlCatch(SDL_bool r);
};

class WarningError : public Error {
public:
	WarningError(const std::string& m) : Error(m) {}
	WarningError(std::string&& m) : Error(m) {}
	~WarningError() = default;

	void raise();
};

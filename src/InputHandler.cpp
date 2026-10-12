#include "InputHandler.h"

bool InputCheck::check(const SDL_Event& e) const {
	if (e.type == t) {
		return f(e);
	}
	return false;
}

InputHold::InputHold(SDL_Scancode s, HoldCallback d) :
	during(d),
	on(false) {
	start = [s] (const SDL_Event& e) { return e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == s; };
	end = [s] (const SDL_Event& e) { return e.type == SDL_EVENT_KEY_UP && e.key.scancode == s; };
}

bool InputHold::check(const SDL_Event& e) {
	if (start(e)) {
		on = true;
		return true;
	}
	if (end(e)) {
		on = false;
		return true;
	}
	return false;
}

void InputHold::update() const {
	if (on) during();
}

void InputHandler::update() {
	/* 
	 * i feel like there should be a more efficient searching algorithm...
	 * to benchmark this we'd need to know what kind of/how many events are
	 * polled each frame
	 */
	while (SDL_PollEvent(&e)) {
		for (const InputCheck& c : checks) {
			c.check(e);
		}
		for (InputHold & h : holds) {
			h.check(e);
		}
	}
	for (const InputHold& h : holds) {
		h.update();
	}
}

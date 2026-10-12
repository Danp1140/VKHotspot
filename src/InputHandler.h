#include <vector>
#include <functional>
#include <iostream>

#include <SDL3/SDL.h>

typedef std::function<bool(const SDL_Event&)> CheckCallback;
typedef std::function<void()> HoldCallback;

class InputCheck {
public:
	InputCheck() = default;
	InputCheck(SDL_EventType et, CheckCallback cf) : t(et), f(cf) {}
	~InputCheck() = default;
	
	/* TODO: consider moving checks to void not bool */
	bool check(const SDL_Event& e) const;

private:
	SDL_EventType t;
	CheckCallback f;
};

class InputHold {
public:
	InputHold() : on(false) {}
	InputHold(CheckCallback s, CheckCallback e, HoldCallback d) :
		start(s),
		end(e),
		during(d),
		on(false) {}
	InputHold(SDL_Scancode s, HoldCallback d);
	~InputHold() = default;

	bool check(const SDL_Event& e);
	void update() const;

private:
	CheckCallback start, end;
	HoldCallback during;
	bool on;
};

class InputHandler {
public:
	InputHandler() = default;
	~InputHandler() = default;

	void update();
	
	void addCheck(InputCheck&& c) {checks.push_back(c);}
	void addHold(InputHold&& h) {holds.push_back(h);}

private:
	std::vector<InputCheck> checks;
	std::vector<InputHold> holds;
	SDL_Event e;
};

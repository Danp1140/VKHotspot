#include "AnimationHandler.h"

void AnimationBase::start() {
	cur_interval_idx = 0;
	offset_into_interval = AnimClock::duration::zero();
	last_time = AnimClock::now();
}

void AnimationHandler::update() {
	std::set<AnimationBase*> to_remove;
	for (AnimationBase* a : playing_animations) {
		if (!a->update(AnimClock::now())) 
			to_remove.insert(a);
	}
	for (AnimationBase* a : to_remove) 
		playing_animations.erase(a);
}

void AnimationHandler::play(AnimationBase* a) {
	playing_animations.insert(a);
	a->start(); // if you add the same multiple times it will just reset the animation
}

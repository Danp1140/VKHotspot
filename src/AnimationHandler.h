#include <chrono>
#include <set>

#include "Errors.h"

/*
	 * What clock to use is is a somewhat important parameter.
	 * We need something steady (monotonic, e.g. no daylight savings issues)
	 * But we also need something with millisecond or finer resolution
	 * For now we will just trust that steady_clock is fine enough
	 */
typedef std::chrono::steady_clock AnimClock;

typedef enum InterpType {
	ANIM_INTERP_TYPE_LINEAR
} InterpType;

typedef struct KeyframeInterval {
	AnimClock::duration dt;
	InterpType interp;
} KeyframeInterval;

class AnimationBase {
public:
	virtual bool update(AnimClock::time_point n) = 0;
	void start();
	
protected:
	AnimClock::duration offset_into_interval;
	AnimClock::time_point last_time;
	size_t cur_interval_idx;
};

template<class T>
class Animation : public AnimationBase {
public:
	Animation() = delete;
	Animation(T& dst) : value_dst(dst) {}

	void setInitialKeyframe(T v) {
		if (keyframes.size() > 0) 
			WarningError("Cannot add initial keyframe when a keyframe already exists").raise();
		else
			keyframes.push_back(v);
	}
	void addKeyframe(T v, float dt) {
		if (keyframes.size() == 0) 
			WarningError("Call setInitialKeyframe to add the first, then call addKeyframe to add additional").raise();
		else {
			keyframes.push_back(v);
			intervals.push_back((KeyframeInterval){
				std::chrono::duration_cast<AnimClock::duration>(std::chrono::duration<float, std::ratio<1>>(dt)), 
				ANIM_INTERP_TYPE_LINEAR});
		}
	}

	// returns true if still going, false if reached end
	// TODO: allow smooth cycling until explicitly told to stop
	bool update(AnimClock::time_point n) {
		if (offset_into_interval > intervals[cur_interval_idx].dt) {
			cur_interval_idx++;
			if (cur_interval_idx == intervals.size()) {
				value_dst = keyframes.back();
				return false;
			}
			else {
				offset_into_interval -= intervals[cur_interval_idx - 1].dt;
			}
		}
		if (intervals[cur_interval_idx].interp == ANIM_INTERP_TYPE_LINEAR) {
			float a = (float)offset_into_interval.count() / (float)intervals[cur_interval_idx].dt.count();
			value_dst = (1-a) * keyframes[cur_interval_idx] + a * keyframes[cur_interval_idx + 1];
		}
		else {
			FatalError("Unrecognized animation keyframe interval interpolation type").raise();
		}
		offset_into_interval += n - last_time;
		last_time = n;
		return true;
	}

private:
	T& value_dst;
	std::vector<T> keyframes;
	std::vector<KeyframeInterval> intervals;
};

class AnimationHandler {
public:
	void update();
	void play(AnimationBase* a);

private:
	std::set<AnimationBase*> playing_animations;
};

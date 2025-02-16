#include <vector>
#include <iostream>

#include <ext.hpp>
#include <portaudio.h>
#include <SDL3/SDL_audio.h> // see if we can get rid of this using a different audio loader

#include "Errors.h"

#define AH_SAMPLE_RATE 44100
#define AH_MAX_CHANNELS 5

#define AH_SPEED_OF_SOUND 343.f // m/s

// doesn't stop overflow, just puts a warning out if it happens
#define AH_CHECK_SAMPLE_OVERFLOW

/*
 * TODO before merge:
 *  - generally cleanup :\
 *  - sync between main thread sound record and audio thread sound record (audiocallbackdata)
 */

class AudioObject;

typedef float (*AudioResponseFunction)(const glm::vec3&, const glm::vec3&);

typedef struct AudioObjectInitInfo {
	glm::vec3 pos = glm::vec3(0),
		forward = glm::vec3(0, 0, 1),
		vel = glm::vec3(0);
	AudioResponseFunction respfunc = nullptr; // if left null set to sphereical rf in constructor
} AudioObjectInitInfo;

class AudioObject {
public:
	AudioObject() : AudioObject((AudioObjectInitInfo){}) {}
	AudioObject(AudioObjectInitInfo ii);

	void setPos(glm::vec3 pos) {p = pos;};
	void setForward(glm::vec3 forw) {f = glm::normalize(forw);};
	void setRespFunc(float (*f)(const glm::vec3&, const glm::vec3&)) {rf = f;}

	const glm::vec3& getPos() const {return p;}
	const glm::vec3& getForward() const {return f;}
	const glm::vec3& getVel() const {return v;}
	const float getResp(const glm::vec3& r) const {return rf(f, r);}

	static float sphericalResponseFunction(const glm::vec3& f, const glm::vec3& r);
	static float hemisphericalResponseFunction(const glm::vec3& f, const glm::vec3& r);


private:
	// velocity v for tracking doppler effect; irrelevent if SPEED_OF_SOUND bit not set in listener
	glm::vec3 p, f, v;
	AudioResponseFunction rf;
};

class Sound : public AudioObject {
public:
	Sound() = delete;
	Sound(const char* fp);

	// TODO: try marking const
	const uint8_t* getBuf() const {return buf;} // managing a data buffer??? gonna need good copy/move constructors ;-;
	uint32_t getBufLen() const {return buflen;}
	const int16_t* getPlayhead() const {return playhead;}
	void advancePlayhead(size_t numsamples) {playhead += numsamples;}

private:
	// buf and buflen are presumed untouched after creation
	// playhead should ONLY be touched by audio thread
	uint8_t* buf;
	uint32_t buflen;
	int16_t* playhead;

	void load();
};

typedef enum ListenerPropBits {
	AH_LISTENER_PROPS_NONE = 0x00,
	AH_LISTENER_PROP_BIT_SPEED_OF_SOUND = 0x01,
	AH_LISTENER_PROP_BIT_INV_SQRT = 0x02
} ListenerPropBits;
typedef uint8_t ListenerProps;

typedef struct ListenerInitInfo {
	AudioObjectInitInfo super;
	std::vector<float> mix = {};
	ListenerProps p = AH_LISTENER_PROPS_NONE;
} ListenerInitInfo;

class Listener : public AudioObject {
public:
	Listener() : Listener((ListenerInitInfo){}) {}
	Listener(ListenerInitInfo ii);

	const float* getMix() const {return &mix[0];}
	float getMix(uint8_t c) const {return mix[c];}
	ListenerProps getProps() const {return props;}

private:
	float mix[AH_MAX_CHANNELS];
	ListenerProps props;
};

class AudioHandler {
public:
	AudioHandler();
	~AudioHandler();

	void start();

	void addSound(Sound&& s);
	void addListener(Listener&& l);
	void addImmediateListener(Listener&& l);

	Sound& getSound(size_t i) {return *sounds[i];}
	const std::vector<Sound*>& getSounds() const {return sounds;}
	Listener& getListener(size_t i) {return *listeners[i];}

private:
	PaStream* stream;
	std::vector<Sound*> sounds;
	std::vector<Listener*> listeners;
	std::mutex mut;
	uint8_t numchannels;

	// below is scratch data to avoid realloc during func invocation
	// this should NEVER be touched by the main thread
	int16_t* outdst, sampletemp;
	const int16_t* playheadtemp;
	float respfactor, lrespfactor, respfactortemp, loffset, offset, offsettemp, dt;
	glm::vec3 diffvec, ldiffvec;
	uint8_t lcount, scount;
	ListenerProps proptemp;
	float mixtemp[AH_MAX_CHANNELS];

	static int streamCallback(
		const void* in, void* out,
		unsigned long framecount,
		const PaStreamCallbackTimeInfo* t,
		PaStreamCallbackFlags flags,
		void *data);

};

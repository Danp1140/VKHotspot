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

typedef float (*AudioResponseFunction)(const glm::vec3&, const glm::vec3&);

typedef struct AudioObjectInitInfo {
	glm::vec3 pos = glm::vec3(0),
		forward = glm::vec3(0, 0, 1);
	AudioResponseFunction respfunc = nullptr;
} AudioObjectInitInfo;

class AudioObject {
public:
	AudioObject() : 
		p(0),
		f(0, 0 ,-1),
		rf(sphericalResponseFunction) {}
	AudioObject(AudioObjectInitInfo ii) :
		p(ii.pos),
		f(ii.forward),
		rf(ii.respfunc) {}

	void setPos(glm::vec3 pos) {p = pos;};
	void setForward(glm::vec3 forw) {f = glm::normalize(forw);};
	void setRespFunc(float (*f)(const glm::vec3&, const glm::vec3&)) {rf = f;}

	const glm::vec3& getPos() const {return p;}
	const glm::vec3& getForward() const {return f;}
	const float getResp(const glm::vec3& r) const {return rf(f, r);}

	static float sphericalResponseFunction(const glm::vec3& f, const glm::vec3& r);
	static float hemisphericalResponseFunction(const glm::vec3& f, const glm::vec3& r);


private:
	glm::vec3 p, f;
	AudioResponseFunction rf;
};

class Sound : public AudioObject {
public:
	Sound() = delete;
	Sound(const char* fp);

	uint8_t* getBuf() {return buf;} // managing a data buffer??? gonna need good copy/move constructors ;-;
	uint32_t getBufLen() {return buflen;}

private:
	uint8_t* buf;
	uint32_t buflen;

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

typedef struct SoundData {
	Sound* s;
	glm::vec3 lp, lf;
} SoundData;

typedef struct ListenerData {
	const Listener* l;
	glm::vec3 lp, lf;
} ListenerData;

typedef struct AudioCallbackData {
	std::vector<ListenerData> listeners, immediatelisteners;
	std::vector<SoundData> sounds;
	uint8_t numchannels;

	// below is scratch data to avoid realloc during func invocation
	int16_t* outdst, sampletemp, * playheadtemp;
	float respfactor, lrespfactor, respfactortemp, loffset, offset, offsettemp;
	glm::vec3 diffvec, ldiffvec;
} AudioCallbackData;

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
	AudioCallbackData acd;

	static int streamCallback(
		const void* in, void* out,
		unsigned long framecount,
		const PaStreamCallbackTimeInfo* t,
		PaStreamCallbackFlags flags,
		void *data);

};

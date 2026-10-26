#include <vector>
#include <iostream>
#include <unordered_set>

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
 *  - sync between main thread sound record and audio thread sound record (audiocallbackdata)
 */

class AudioObject;

typedef uint8_t AOIndex;
typedef unsigned long FrameSampleIndex;
typedef uint8_t ChannelIndex;
typedef int16_t Sample;
typedef uint8_t ListenerIndex;
typedef uint8_t SoundIndex;
typedef float (*AudioResponseFunction)(const glm::vec3&, const glm::vec3&);

typedef struct AudioObjectInitInfo {
	glm::vec3 pos = glm::vec3(0),
		forward = glm::vec3(0, 0, 1),
		vel = glm::vec3(0);
	AudioResponseFunction respfunc = nullptr; // if left null set to spherical rf in constructor
} AudioObjectInitInfo;

class AudioObject {
public:
	AudioObject() : AudioObject((AudioObjectInitInfo){}) {}
	AudioObject(AudioObjectInitInfo ii);

	void setPos(glm::vec3 pos) {p = pos;}
	void setForward(glm::vec3 forw) {f = glm::normalize(forw);}
	void setVel(glm::vec3 vel) {v = vel;}
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
	// TODO: copy/move constructors, nontrivial b/c of buf
	Sound(const char* fp);
	// src is channel-interlaced audio samples
	// len is size in bytes of entire buffer
	Sound(Sample* src, size_t len) : buf(src), buflen(len), playhead(src) {}

	const Sample* getBuf() const {return buf;} 
	uint32_t getBufLen() const {return buflen;}
	const Sample* getPlayhead() const {return playhead;}
	void advancePlayhead(size_t numsamples) {playhead += numsamples;}

private:
	// buf and buflen are presumed untouched after creation
	// playhead should ONLY be touched by audio thread
	Sample* buf;
	uint32_t buflen;
	Sample* playhead;

	void load();
};

class MultiChannelSound : public Sound {
public:	
	MultiChannelSound() = delete;
	MultiChannelSound(const char* fp, ChannelIndex nc) : Sound(fp), n_channels(nc) {}

	ChannelIndex getNChannels() const {return n_channels;}	
private:
	ChannelIndex n_channels;
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
	float getMix(ChannelIndex c) const {return mix[c];}
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
	void addDirectSound(const MultiChannelSound& s);
	void addListener(Listener&& l);

	const std::unordered_set<Sound*>& getSounds() const {return sounds;}
	const std::unordered_set<MultiChannelSound*>& getDirectSounds() const {return direct_sounds;}
	Listener& getListener(ListenerIndex i) {return *listeners[i];}

private:
	PaStream* stream;
	std::unordered_set<Sound*> sounds;
	std::unordered_set<MultiChannelSound*> direct_sounds;
	std::vector<Listener*> listeners;
	ChannelIndex numchannels;

	std::mutex sounds_mut, direct_sounds_mut, listeners_mut;

	// below is scratch data to avoid realloc during func invocation
	// this should NEVER be touched by the main thread
	Sample* outdst, sampletemp;
	const Sample* playheadtemp;
	float respfactor, lrespfactor, respfactortemp, loffset, offset, offsettemp, dt;
	glm::vec3 diffvec, ldiffvec;
	ListenerIndex lcount;
	SoundIndex scount;
	std::unordered_set<Sound*> stokill;
	ListenerProps proptemp;
	float mixtemp[AH_MAX_CHANNELS];
	bool listenerdone, soundongoing;

	static int streamCallback(
		const void* in, void* out,
		unsigned long framecount,
		const PaStreamCallbackTimeInfo* t,
		PaStreamCallbackFlags flags,
		void *data);

};

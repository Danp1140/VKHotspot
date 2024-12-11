#include <vector>
#include <iostream>

#include <ext.hpp>
#include <portaudio.h>
#include <SDL3/SDL_audio.h> // see if we can get rid of this using a different audio loader

#include "Errors.h"

#define AH_SAMPLE_RATE 44100
#define AH_MAX_CHANNELS 5

#define AH_SPEED_OF_SOUND 343.f // m/s

class AudioObject {
public:
	AudioObject() : 
		p(0),
		f(0, 0 ,-1),
		rf(sphericalResponseFunction) {}

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
	float (*rf)(const glm::vec3&, const glm::vec3&);
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

class Listener : public AudioObject {
public:
	Listener();
	Listener(std::vector<float> m);

	uint8_t getNumChannels() const {return numchannels;}
	const float* getMix() const {return &mix[0];}
	float getMix(uint8_t c) const {return mix[c];}

private:
	uint8_t numchannels;
	float mix[AH_MAX_CHANNELS];
};

typedef struct SoundData {
	Sound* s;
	glm::vec3 lp, lf;
	int16_t* playhead; // TODO; see if we can phase this out
} SoundData;

typedef struct RelativityData {
	float offset;
	float offsetrate; // in samples/sample
} RelativityData;

typedef struct ListenerData {
	const Listener* l;
	glm::vec3 lp, lf;
	// TODO: do away with rel data, recalculated every invoc anyway
	std::vector<RelativityData> rel; // TODO: can we make the vector but not the data within const???
} ListenerData;

typedef struct AudioCallbackData {
	std::vector<ListenerData> listeners;
	std::vector<SoundData> sounds;
} AudioCallbackData;

class AudioHandler {
public:
	AudioHandler();
	~AudioHandler();

	void start();

	void addSound(Sound&& s);
	void addListener(Listener&& l);

	// TODO: maybe a string-value map would be more intuitive
	Sound& getSound(size_t i) {return *sounds[i];}
	const std::vector<Sound*>& getSounds() const {return sounds;}
	Listener& getListener(size_t i) {return *listeners[i];}

private:
	PaStream* stream;
	std::vector<Sound*> sounds;
	std::vector<Listener*> listeners;
	// immediatelisteners (no speed of sound delay)
	// directlisteners (no geometry considerations whatsoever)
	// BIG TODO: gain system
	AudioCallbackData acd;

	// TODO: separate callback for sounds where speed of sound is negligible
	// and for sounds where the listener/sound geometry is unimportant
	static int streamCallback(
		const void* in, void* out,
		unsigned long framecount,
		const PaStreamCallbackTimeInfo* t,
		PaStreamCallbackFlags flags,
		void *data);

};

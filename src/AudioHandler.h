#include <vector>
#include <iostream>

#include <ext.hpp>
#include <portaudio.h>

#define AH_SAMPLE_RATE 44100

class AudioObject {
private:
	glm::vec3 p, f;
	uint16_t (*rf)(glm::vec3 r);

	static uint16_t sphericalResponseFunction(glm::vec3 r);
};

class Sound : public AudioObject {
private:

	void load();
	void play();
};

class Listener : public AudioObject {
	// TODO: channel mix scales
private:
};

typedef struct TestData {
	float l = 0, r = 0;
} TestData;

typedef struct AudioCallbackData {
	float* playhead;
	std::vector<size_t> offsets;
	// TODO: way for offset to vary over frame
	// for doppler shift, this system would result in skipping, not sliding
	// could probably just do this by sending pairs of offsets and doing a standard mix
	// over the duration of the frame
	// requires us to know how long the frame is before it processes though
	// with an unknown buffer size, we could just dictate an initial offset and an offset change rate per
	// sample in the frame, as we know the time between each sample, guarenteed
} AudioCallbackData;

class AudioHandler {
public:
	AudioHandler();
	~AudioHandler();

private:
	PaStream* stream;
	std::vector<Sound> sounds;
	std::vector<Listener> listeners;
	TestData d;

	static int streamCallback(
		const void* in, void* out,
		unsigned long framecount,
		const PaStreamCallbackTimeInfo* t,
		PaStreamCallbackFlags flags,
		void *data);

};

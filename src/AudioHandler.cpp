#include "AudioHandler.h"

uint16_t AudioObject::sphericalResponseFunction(glm::vec3 r) {
	return UINT16_MAX;
}

AudioHandler::AudioHandler() {
	// TODO: allow for multiple AH's, don't always initialize PA
	Pa_Initialize();
	int numdev = Pa_GetDeviceCount();
	for (int i = 0; i < numdev; i++) {
		std::cout << Pa_GetDeviceInfo(i)->name << std::endl;
	}
	// TODO: device querying
	Pa_OpenDefaultStream(
		&stream,
		0, 2,
		paFloat32, // TODO: consider other sample formats
		AH_SAMPLE_RATE,
		paFramesPerBufferUnspecified,
		streamCallback,
		&d);
	// TODO: may want to allow for delayed AH start
	Pa_StartStream(stream);
}

AudioHandler::~AudioHandler() {
	Pa_StopStream(stream);
	Pa_CloseStream(stream);
	Pa_Terminate();
}

int AudioHandler::streamCallback(
		const void* in, void* out,
		unsigned long framecount,
		const PaStreamCallbackTimeInfo* t,
		PaStreamCallbackFlags flags,
		void *data) {
	float* outdst = static_cast<float*>(out);
	TestData* d = static_cast<TestData*>(data);
	for (unsigned long i = 0; i < framecount; i++) {
		*outdst++ = sin(d->l);
		*outdst++ = sin(d->r);
		d->l += 0.1;
		d->r += 0.1f * pow(2.f, 4.f / 12.f);
	}
	std::cout << "here " << std::endl;
	return 0;
}

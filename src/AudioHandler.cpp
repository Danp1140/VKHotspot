#include "AudioHandler.h"

float AudioObject::sphericalResponseFunction(const glm::vec3& f, const glm::vec3& r) {
	return 1;
}

float AudioObject::hemisphericalResponseFunction(const glm::vec3& f, const glm::vec3& r) {
	// TODO; determine pre-normalization conventions, this function runs a lot
	// return std::max(0.f, glm::dot(f, glm::normalize(r)));
	float res = glm::dot(f, glm::normalize(r));
	return res > 0 ? res : 0;
}

Sound::Sound() {
	SDL_AudioSpec temp;
	FatalError("Bad LoadWAV").sdlCatch(SDL_LoadWAV("../resources/sounds/ta1.1mono.wav", &temp, &buf, &buflen));
}

Listener::Listener() : numchannels(2) {
	for (uint8_t c = 0; c < numchannels; c++) mix[c] = 1;
}

AudioHandler::AudioHandler() {
	// TODO: allow for multiple AH's, don't always initialize PA
	Pa_Initialize();
	/*
	int numdev = Pa_GetDeviceCount();
	for (int i = 0; i < numdev; i++) {
		std::cout << Pa_GetDeviceInfo(i)->name << std::endl;
	}
	*/
}

AudioHandler::~AudioHandler() {
	for (Sound* s : sounds) delete s;
	for (Listener* l : listeners) delete l;

	Pa_StopStream(stream);
	Pa_CloseStream(stream);
	Pa_Terminate();
}

void AudioHandler::start() {
	// TODO: device querying
	Pa_OpenDefaultStream(
		&stream,
		0, 2,
		paInt16,
		AH_SAMPLE_RATE,
		paFramesPerBufferUnspecified,
		streamCallback,
		&acd);
	Pa_StartStream(stream);
}

void AudioHandler::addSound(Sound&& s) {
	sounds.push_back(new Sound(s));
	acd.sounds.push_back({
		sounds.back(),
		sounds.back()->getPos(), sounds.back()->getForward(),
		reinterpret_cast<int16_t*>(sounds.back()->getBuf()),
	});
	for (ListenerData& l : acd.listeners) {
		l.rel.push_back({
			glm::distance(sounds.back()->getPos(), l.l->getPos()) / AH_SPEED_OF_SOUND * (float)AH_SAMPLE_RATE,
			0
		});
	}
}

void AudioHandler::addListener(Listener&& l) {
	listeners.push_back(new Listener(l));
	acd.listeners.push_back({
		listeners.back(),
		listeners.back()->getPos(), listeners.back()->getForward(),
		{}
	});
	for (Sound* s : sounds) {
		acd.listeners.back().rel.push_back({
			glm::distance(sounds.back()->getPos(), listeners.back()->getPos()) / AH_SPEED_OF_SOUND * (float)AH_SAMPLE_RATE,
			0
		});
	}
}

int AudioHandler::streamCallback(
		const void* in, void* out,
		unsigned long framecount,
		const PaStreamCallbackTimeInfo* t,
		PaStreamCallbackFlags flags,
		void *data) {
	AudioCallbackData* d = static_cast<AudioCallbackData*>(data);
	// could technically avoid all this alloc by having these values in the data ptr
	int16_t* outdst, sampletemp;
	float respfactor, lrespfactor, respfactortemp;
	glm::vec3 diffvec, ldiffvec;
	/*
	float dt = -(float)framecount / (float)AH_SAMPLE_RATE,
	      timeoffset;
	      */
	// watch out for overflow, although >256 listeners and/or sounds is criminal
	for (uint8_t li = 0; li < d->listeners.size(); li++) {
		for (uint8_t si = 0; si < d->sounds.size(); si++) {
			// timeoffset = dist / AH_SPEED_OF_SOUND;
			outdst = static_cast<int16_t*>(out);
			/*
			 * diffvec and values derived therefrom will vary per-sample, but may be
			 * too expensive to calculate in that way. i estimate frame sizes varying
			 * from 256 to 4096, or 0.006 to 0.093 seconds. so, i think we can calculate
			 * a start, an end, and do linear interpolation (even if the values do not actually
			 * vary linearly).
			 */
			// points from listener to sound
			diffvec = d->sounds[si].s->getPos() - d->listeners[li].l->getPos();
			ldiffvec = d->sounds[si].lp - d->listeners[li].lp;
			// maybe a time for fast inverse square root??
			respfactor = pow(glm::length(diffvec), -0.5)
				 * d->listeners[li].l->getResp(diffvec) 
				 * d->sounds[si].s->getResp(-diffvec);
			lrespfactor = pow(glm::length(ldiffvec), -0.5)
				 * d->listeners[li].l->getResp(ldiffvec) 
				 * d->sounds[si].s->getResp(-ldiffvec);
			// std::cout << lrespfactor << " -> " << respfactor << std::endl;
			/*
			std::cout << pow(glm::length(diffvec), -0.5)
				 << ", " << d->listeners[li].l->getResp(diffvec) 
				 << ", " <<  d->sounds[si].s->getResp(-diffvec) << std::endl;
				 */

			for (unsigned long i = 0; i < framecount; i++) {
				sampletemp = std::lerp(
						*(d->sounds[si].playhead + (size_t)floor(d->listeners[li].rel[si].offset)),
						*(d->sounds[si].playhead + (size_t)ceil(d->listeners[li].rel[si].offset)), 
						fmod(d->listeners[li].rel[si].offset, 1));
				respfactortemp = std::lerp(lrespfactor, respfactor, (float)i / (float)framecount);
				for (uint8_t c = 0; c < d->listeners[li].l->getNumChannels(); c++) { // technically this must be the same for all 
					*outdst++ = 
						sampletemp 
						 * d->listeners[li].l->getMix(c) 
						 * respfactortemp;
				}
				d->sounds[si].playhead++;
				d->listeners[li].rel[si].offset += d->listeners[li].rel[si].offsetrate;
			}
			if (li == d->listeners.size() - 1) {
				d->sounds[si].lp = d->sounds[si].s->getPos();
				d->sounds[si].lf = d->sounds[si].s->getForward();
			}
		}
		d->listeners[li].lp = d->listeners[li].l->getPos();
		d->listeners[li].lf = d->listeners[li].l->getForward();
	}
	return 0;
}

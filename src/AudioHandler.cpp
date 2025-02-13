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

Sound::Sound(const char* fp) {
	SDL_AudioSpec temp;
	FatalError("Bad LoadWAV").sdlCatch(SDL_LoadWAV(fp, &temp, &buf, &buflen));

}

Listener::Listener(ListenerInitInfo ii) :
	AudioObject(ii.super),
	props(ii.p) {
	// TODO: audiostream num channels, not just total max channels
	if (ii.mix.size() > AH_MAX_CHANNELS) 
		WarningError("Too many audio channels, taking as many as possible").raise();
	for (uint8_t c = 0; c < std::min(ii.mix.size(), (size_t)AH_MAX_CHANNELS); c++) mix[c] = ii.mix[c];
}

AudioHandler::AudioHandler() {
	// TODO: allow for multiple AH's, don't always initialize PA
	Pa_Initialize();
	int numdev = Pa_GetDeviceCount();
	for (int i = 0; i < numdev; i++) {
		std::cout << Pa_GetDeviceInfo(i)->name << std::endl;
	}
}

AudioHandler::~AudioHandler() {
	for (Sound* s : sounds) delete s;
	for (Listener* l : listeners) delete l;

	Pa_StopStream(stream);
	Pa_CloseStream(stream);
	Pa_Terminate();
}

void AudioHandler::start() {
	acd.numchannels = 2;
	// TODO: device querying, also allows setting channels
	Pa_OpenDefaultStream(
		&stream,
		0, acd.numchannels,
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

/*
 * diffvec and values derived therefrom will vary per-sample, but may be
 * too expensive to calculate in that way. i estimate frame sizes varying
 * from 256 to 4096, or 0.006 to 0.093 seconds. so, i think we can calculate
 * a start, an end, and do linear interpolation (even if the values do not actually
 * vary linearly).
 *
 * the reason I think it's good to do lerp instead of just taking a value is that we
 * don't actually know exactly how small the frames are, and a jump in level, especially
 * a large one at an unlucky time, could be very obvious
 */
int AudioHandler::streamCallback(
		const void* in, void* out,
		unsigned long framecount,
		const PaStreamCallbackTimeInfo* t,
		PaStreamCallbackFlags flags,
		void *data) {
	AudioCallbackData* d = static_cast<AudioCallbackData*>(data);
	memset(out, 0, framecount * d->numchannels * sizeof(int16_t));
	
	// watch out for overflow, although >256 listeners and/or sounds is criminal
	for (uint8_t li = 0; li < d->listeners.size(); li++) {
		for (uint8_t si = 0; si < d->sounds.size(); si++) {
			d->outdst = static_cast<int16_t*>(out);
			d->playheadtemp = d->sounds[si].playhead;
			// TODO: conslidate d->diffvec length calcs as stored float
			if (d->listeners[li].l->getProps() & (AH_LISTENER_PROP_BIT_SPEED_OF_SOUND | AH_LISTENER_PROP_BIT_INV_SQRT)) {
				d->diffvec = d->sounds[si].s->getPos() - d->listeners[li].l->getPos();
				d->ldiffvec = d->sounds[si].lp - d->listeners[li].lp;
				d->respfactor = d->listeners[li].l->getResp(d->diffvec) 
					 * d->sounds[si].s->getResp(-d->diffvec);
				d->lrespfactor = d->listeners[li].l->getResp(d->diffvec) 
					 * d->sounds[si].s->getResp(-d->diffvec);
				if (d->listeners[li].l->getProps() & AH_LISTENER_PROP_BIT_INV_SQRT) {
					d->respfactor *= pow(glm::length(d->diffvec), -0.5);
					d->lrespfactor *= pow(glm::length(d->ldiffvec), -0.5);
				}
				if (d->listeners[li].l->getProps() & AH_LISTENER_PROP_BIT_SPEED_OF_SOUND) {
					d->offset = glm::length(d->diffvec) / AH_SPEED_OF_SOUND * AH_SAMPLE_RATE;
					d->loffset = glm::length(d->ldiffvec) / AH_SPEED_OF_SOUND * AH_SAMPLE_RATE;
				}
			}
			for (unsigned long i = 0; i < framecount; i++) {
				if (si == d->sounds.size() - 1 
					 && (uint8_t*)d->playheadtemp - d->sounds[si].s->getBuf() == d->sounds[si].s->getBufLen()) {
					// TODO: end of sound handling
					// involves sync with main thread
					// erase sound
					// main thread shoud check sound struct (read only) to update AH's main
					// record of sounds
				}
				if (d->listeners[li].l->getProps() & AH_LISTENER_PROP_BIT_SPEED_OF_SOUND) {
						d->offsettemp = std::lerp(d->loffset, d->offset, (float)i / (float)framecount);
					// TODO: see if you can adjust the math to remove a subtraction of unsigned values
					if ((uint8_t*)(d->playheadtemp - (size_t)ceil(d->offsettemp)) < d->sounds[si].s->getBuf())
						d->sampletemp = 0;
					else 
						d->sampletemp = std::lerp(
							*(d->playheadtemp - (size_t)floor(d->offsettemp)),
							*(d->playheadtemp - (size_t)ceil(d->offsettemp)), 
							fmod(d->offsettemp, 1));
				}
				else {
					d->sampletemp = *d->playheadtemp;
				}
				d->respfactortemp = std::lerp(d->lrespfactor, d->respfactor, (float)i / (float)framecount);
				for (uint8_t c = 0; c < d->numchannels; c++) { 
#ifdef AH_SPEED_OF_SOUND
					if (INT16_MAX - *d->outdst < d->sampletemp * d->listeners[li].l->getMix(c) * d->respfactortemp) 
						WarningError("audio sample overflow").raise();
					if (INT16_MIN - *d->outdst > d->sampletemp * d->listeners[li].l->getMix(c) * d->respfactortemp) 
						WarningError("audio sample underflow").raise();
#endif
					*d->outdst += 
						d->sampletemp 
						 * d->listeners[li].l->getMix(c) 
						 * d->respfactortemp;
					d->outdst++;
				}
				d->playheadtemp++;
			}
			if (li == d->listeners.size() - 1) {
				d->sounds[si].lp = d->sounds[si].s->getPos();
				d->sounds[si].lf = d->sounds[si].s->getForward();
				d->sounds[si].playhead += framecount;
			}
		}
		d->listeners[li].lp = d->listeners[li].l->getPos();
		d->listeners[li].lf = d->listeners[li].l->getForward();
	}
	return 0;
}

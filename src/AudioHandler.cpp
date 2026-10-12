#include "AudioHandler.h"

AudioObject::AudioObject(AudioObjectInitInfo ii) :
		p(ii.pos),
		f(ii.forward),
		rf(ii.respfunc) {
	if (!rf) rf = sphericalResponseFunction;
}

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
	playhead = reinterpret_cast<int16_t*>(buf);
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
	numchannels = 2;
	// TODO: device querying, also allows setting channels
	Pa_OpenDefaultStream(
		&stream,
		0, numchannels,
		paInt16,
		AH_SAMPLE_RATE,
		paFramesPerBufferUnspecified,
		streamCallback,
		this);
	Pa_StartStream(stream);
}

void AudioHandler::addSound(Sound&& s) {
	mut.lock();
	sounds.insert(new Sound(s));
	mut.unlock();
}

void AudioHandler::addListener(Listener&& l) {
	mut.lock();
	listeners.push_back(new Listener(l));
	mut.unlock();
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
	AudioHandler* d = static_cast<AudioHandler*>(data);
	memset(out, 0, framecount * d->numchannels * sizeof(int16_t));
	d->dt = (float)framecount / AH_SAMPLE_RATE;
	d->stokill.clear();

	// Thread safety notes:
	// 
	// We assume a sound's buffer and bufferlength to be untouched after creation
	
	// to avoid stopping main thread for long, have add/remove funcs dispatch separate thread
	// TODO: deadlock insurance
	d->mut.lock();
	d->lcount = d->listeners.size();
	d->scount = d->sounds.size();
	// watch out for overflow, although >256 listeners and/or sounds is criminal
	for (uint8_t li = 0; li < d->lcount; li++) {
		d->proptemp = d->listeners[li]->getProps();
		memcpy(d->mixtemp, d->listeners[li]->getMix(), d->numchannels * sizeof(float));
		for (Sound* s : d->sounds) {
			d->outdst = static_cast<int16_t*>(out);
			d->playheadtemp = s->getPlayhead();
			d->soundongoing = false;
			// TODO: conslidate d->diffvec length calcs as stored float
			if (d->proptemp & (AH_LISTENER_PROP_BIT_SPEED_OF_SOUND | AH_LISTENER_PROP_BIT_INV_SQRT)) {
				d->diffvec = s->getPos() - d->listeners[li]->getPos();
				// d->ldiffvec = s->lp - d->listeners[li]->lp;
				d->ldiffvec = s->getPos() - d->listeners[li]->getPos() - d->dt * (s->getVel() - d->listeners[li]->getVel());
				d->respfactor = d->listeners[li]->getResp(d->diffvec) 
					 * s->getResp(-d->diffvec);
				d->lrespfactor = d->listeners[li]->getResp(d->ldiffvec) 
					 * s->getResp(-d->ldiffvec);
				if (d->proptemp & AH_LISTENER_PROP_BIT_INV_SQRT) {
					d->respfactor *= pow(glm::length(d->diffvec), -0.5);
					d->lrespfactor *= pow(glm::length(d->ldiffvec), -0.5);
				}
				if (d->proptemp & AH_LISTENER_PROP_BIT_SPEED_OF_SOUND) {
					d->offset = glm::length(d->diffvec) / AH_SPEED_OF_SOUND * AH_SAMPLE_RATE;
					d->loffset = glm::length(d->ldiffvec) / AH_SPEED_OF_SOUND * AH_SAMPLE_RATE;
				}
			}
			d->listenerdone = false;
			for (unsigned long i = 0; i < framecount; i++) {
				if ((uint8_t*)d->playheadtemp - s->getBuf() == s->getBufLen()) {
					d->listenerdone = true;
					std::cout << "listener done" << std::endl;
					break;
				}
				if (d->proptemp & AH_LISTENER_PROP_BIT_SPEED_OF_SOUND) {
					d->offsettemp = std::lerp(d->loffset, d->offset, (float)i / (float)framecount);
					// TODO: see if you can adjust the math to remove a subtraction of unsigned values
					if ((uint8_t*)(d->playheadtemp - (size_t)ceil(d->offsettemp)) < s->getBuf())
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
					if (INT16_MAX - *d->outdst < d->sampletemp * d->mixtemp[c] * d->respfactortemp) 
						WarningError("audio sample overflow").raise();
					if (INT16_MIN - *d->outdst > d->sampletemp * d->mixtemp[c] * d->respfactortemp) 
						WarningError("audio sample underflow").raise();
#endif
					*d->outdst += 
						d->sampletemp 
						 * d->mixtemp[c] 
						 * d->respfactortemp;
					d->outdst++;
				}
				d->playheadtemp++;
			}
			if (!d->listenerdone) d->soundongoing = true;
			if (li == d->lcount - 1) {
				// if (!d->soundongoing) d->sounds.erase(s);
				// sketchy: we will advance playhead past end of valid data because SOS listeners
				// may not be done yet and still require offset info
				// data will not actually be read though, that check is done in the sample read loop
				if (!d->soundongoing) d->stokill.insert(s);
				else s->advancePlayhead(framecount);
			}
		}
	}
	d->mut.unlock();
	if (!d->stokill.empty()) {
		d->mut.lock();
		for (Sound* k : d->stokill) {
			d->sounds.erase(k);
			std::cout << "killed " << k << std::endl;
		}
		d->mut.unlock();
	}
	return 0;
}

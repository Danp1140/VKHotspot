#include <map>
#include <fts.h>
#include <random>

#include <png.h>

#include "GraphicsHandler.h"

// #define VERBOSE_TEXTURESET_OBJECTS

// TODO: prob a better way to account for pi lol
#define PI 3.14159

/*
class OnceStep {
public:
	virtual void generate(void* dst) = 0;

private:
	VkExtent2D ext;
};

class OSBrownian : public OnceStep {
public:
	void generate(void* dst);

private:
	uint64_t seed;
	uint8_t depth;
};
*/

/*
 * GenSteps are deterministic, so order of execution does not matter
 */
template<class T>
class GenStep {
public:
	// TODO: should in be const as well???
	virtual T generate(T* in) const = 0;

private:

};

template<class U>
class GSConst : public GenStep<U> {
public:
	GSConst(U v) : value(v) {}
	U generate(U* in) const {return value;}

private:
	U value;
};

template<class U>
class GSAdd : public GenStep<U> {
public:
	U generate(U* in) const {return in[0] + in[1];}
};

template<class U>
class GSMultiply : public GenStep<U> {
public:
	U generate(U* in) const {return in[0] * in[1];}
};

template<class U>
class GSModu : public GenStep<U> {
public:
	U generate(U* in) const {return mod(in[0], in[1]);}

	static U mod(const U& a, const U& b) {
		if constexpr (std::is_floating_point_v<U>) return fmod(a, b);
		else return a % b;
	}
};

typedef enum GSSampleType {
	GS_SAMPLE_TYPE_NEAREST,
	GS_SAMPLE_TYPE_LINEAR
} GSSampleType;

template<class U>
class GSSample : public GenStep<U> {
public:
	GSSample(GSSampleType t) : type(t) {}

	U generate(U* in) const {
		if (type == GS_SAMPLE_TYPE_NEAREST) {

		}
	}

private:
	GSSampleType type;
};

/*
 * Non-"FAST" types are scaled to cross (0, 0), (2n + 0.5, 1), (2n + 1, 0), and (2n + 1.5, -1)
 * It is advised that one uses the "FAST" versions, which apply no scaling, when convenient
 */
typedef enum GSOscType {
	GS_OSC_TYPE_SINE,
	GS_OSC_TYPE_SAW,
	GS_OSC_TYPE_TRI,
	GS_OSC_TYPE_SQUARE,
	GS_OSC_TYPE_SINE_FAST, // period of 2pi, range [-1, 1] 
	GS_OSC_TYPE_SAW_FAST, // period of 1, range [0, 1]
	GS_OSC_TYPE_TRI_FAST, // period of 1, range [0, 0.5]
	GS_OSC_TYPE_SQUARE_FAST
} GSOscType;

template<class U>
class GSOsc : public GenStep<U> {
public:
	GSOsc(GSOscType t) : type(t) {}

	// TODO: consider rescaling to make defaults work with integral types
	U generate(U* in) const {
		if (type == GS_OSC_TYPE_SINE_FAST) return sin(in[0]);
		if (type == GS_OSC_TYPE_SAW_FAST) return GSModu<U>::mod(in[0], 1); 
		if (type == GS_OSC_TYPE_TRI_FAST) return abs(GSModu<U>::mod(in[0], 1) - 0.5); 
		if (type == GS_OSC_TYPE_SQUARE_FAST || type == GS_OSC_TYPE_SQUARE) {
			// TODO: test this with narrowing casts
			// since we're just doing parity hopefully most implementations this will be efficient and work
			if (static_cast<uint8_t>(ceil(in[0])) % static_cast<uint8_t>(2) == 1) return 1;
			else return 0;
		}
		if (type == GS_OSC_TYPE_SINE) return sin(PI * in[0]);
		if (type == GS_OSC_TYPE_SAW) return 2 * GSModu<U>::mod(in[0] + 0.5, 1) - 1;
		if (type == GS_OSC_TYPE_TRI) return 2 * abs(2 * GSModu<U>::mod(in[0] / 2 - 0.25, 1) - 1) - 1;
		FatalError("Undefined oscillator type").raise();
		return 0;
	}

private:
	GSOscType type;
};

template<class U>
class GSLoad : public GenStep<U> {
public:
	GSLoad(const U* d) : data(d) {}

	U generate(U* in) const {return data[in[0]];}

private:
	const U* data;
};

// Just like a GenStep but non-deterministic, so 
template<class T>
class NDStep {
public:
	virtual T generate(T* in) = 0;
};

// TODO: noise type
// both dist and number type
// TODO: seeding
template<class U>
class NDSNoise : public NDStep<U> {
public:
	NDSNoise() : dist(std::numeric_limits<U>::min(), std::numeric_limits<U>::max()) {}
	NDSNoise(U a, U b) : dist(a, b) {}

	U generate(U* in) {return dist(gen);}

private:
	std::mt19937 gen;
	std::uniform_int_distribution<U> dist;
};

#define SN_PIXEL_ID nullptr

template<class T>
class GenStepNode {
public:
	virtual T generate(size_t pi) const = 0;
};

template<class T>
class StepNode : public GenStepNode<T> {
public:
	StepNode(const std::vector<const GenStepNode<T>*>& i, const GenStep<T>* g) : ins(i), gs(g) {}
	StepNode(std::vector<const GenStepNode<T>*>&& i, const GenStep<T>* g) : ins(std::move(i)), gs(g) {}

	T generate(size_t pi) const {
		T ingens[ins.size()];
		for (size_t i = 0; i < ins.size(); i++) {
			if (ins[i] == SN_PIXEL_ID) ingens[i] = static_cast<T>(pi);
			else ingens[i] = ins[i]->generate(pi);
		}
		return gs->generate(&ingens[0]);
	}
	
private:
	std::vector<const GenStepNode<T>*> ins;
	const GenStep<T>* gs;
};

template<class T, class U>
class CastStepNode : public GenStepNode<T> {
public:
	CastStepNode(const GenStepNode<U>* i) : in(i) {}
	
	T generate (size_t pi) const {
		if (in == SN_PIXEL_ID) return static_cast<T>(pi);
		return static_cast<T>(in->generate(pi));
	}

private:
	const GenStepNode<U>* in;
};

class TextureSet {
public:
	TextureSet() = default;
	TextureSet(const TextureSet& lvalue) = delete;
	TextureSet(TextureSet&& rvalue);
	// TODO: constructor allowing different sampler for each tex
	TextureSet(const char* d, VkSampler s);
	~TextureSet();

	friend void swap(TextureSet& lhs, TextureSet& rhs);

	TextureSet& operator=(const TextureSet& rhs) = delete;
	TextureSet& operator=(TextureSet&& rhs);

	// could also make this operator[]
	const ImageInfo& getTexture(std::string n) const {return textures.at(n);}
	template <class T>
	void addTexture(std::string n, const std::vector<GenStepNode<T>*>& roots, size_t res, VkFormat fmt, VkSampler s) {
		ImageInfo* dst;
		dst = &textures.insert({n, {}}).first->second;
		dst->extent = {static_cast<uint32_t>(res), static_cast<uint32_t>(res)};
		dst->format = fmt;
		dst->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		dst->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		dst->sampler = s; 
		GH::createImage(*dst);

		void* data = malloc(res*res*sizeof(T)*roots.size());
		T* scan = (T*)data;
		/*
		for (uint32_t i = 0; i < res; i++) {
			for (uint32_t j = 0; j < res; j++) {
				for (uint8_t k = 0; k < roots.size(); k++) {
					*scan++ = roots[k]->generate(i * res + j);
				}
			}
		}
		*/
		for (size_t i = 0; i < res * res; i++) {
			for (uint8_t k = 0; k < roots.size(); k++) {
				*scan++ = roots[k]->generate(i);
			}
		}
		GH::updateImage(*dst, data);
		free(data);
	}

private:
	std::map<std::string, ImageInfo> textures;

	/* for move operations, keeps vulkan image objects from being deleted */
	void nukeTextures();

	/*
	template<class T>
	static std::vector<StepNode<T>>
	*/
};

class TextureHandler {
public:
	TextureHandler() : defaultsampler(VK_NULL_HANDLE) {}
	~TextureHandler();

	const TextureSet& getSet(std::string n) const {return sets.at(n);}
	VkSampler getSampler(std::string n) {return samplers.at(n);}
	VkSampler getDefaultSampler() {return defaultsampler;}

	// TODO: allow adding with a VkSampler too, perhaps a list or map of them
	void addSet(std::string n, TextureSet&& t);
	VkSampler addSampler(
		const char* n, 
		VkFilter mag, VkFilter min, 
		VkSamplerAddressMode addr, 
		VkBool32 aniso);
	void setDefaultSampler(VkSampler s) {defaultsampler = s;}
private:
	/*
	 * okay, so i think the most expedient solution is to store a vector/array/set
	 * of heap-alloc'd texture sets (alloc'd and data moved in addSet), and a map
	 * btwn std::string and heap alloc'd ptr
	 */
	std::map<std::string, TextureSet> sets;
	std::map<std::string, VkSampler> samplers;
	VkSampler defaultsampler;
};

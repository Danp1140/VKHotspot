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
	virtual ~GenStep() = default;
	virtual T generate(const std::vector<size_t>& p) const = 0;
};

template<class T>
class GSConst : public GenStep<T> {
public:
	GSConst(T v) : value(v) {}
	~GSConst() = default;
	T generate(const std::vector<size_t>& p) const {return value;}

private:
	T value;
};

class GSDim : public GenStep<size_t> {
public:
	GSDim(uint8_t i) : dim_idx(i) {}
	~GSDim() = default;
	size_t generate(const std::vector<size_t>& p) const {return p[dim_idx];}

private:
	uint8_t dim_idx;
};

template<class T, class U>
class GSCast : public GenStep<T> {
public:
	GSCast(GenStep<U>* c) : castee(c) {}
	~GSCast() {
		if (castee) delete castee;
	}
	T generate(const std::vector<size_t>& p) const {return static_cast<T>(castee->generate(p));}
private:
	GenStep<U>* castee;
};

typedef enum GSBinOpType {
	GS_BINOP_TYPE_ADD,
	GS_BINOP_TYPE_MULT,
	GS_BINOP_TYPE_MOD,
} GSBinOpType;

// COULD make this a multiple template, with the other being the binop type for max efficiency
// probably not a bottleneck for a long time

template<class T>
class GSBinOp : public GenStep<T> {
public:
	GSBinOp(GSBinOpType t, GenStep<T>* l, GenStep<T>* r) : op(t), lhs(l), rhs(r) {}
	~GSBinOp() {
		if (lhs) delete lhs;
		if (rhs) delete rhs;
	}
	T generate(const std::vector<size_t>& p) const {
		switch (op) {
			case GS_BINOP_TYPE_ADD:
				return lhs->generate(p) + rhs->generate(p);
			case GS_BINOP_TYPE_MULT:
				return lhs->generate(p) * rhs->generate(p);
			case GS_BINOP_TYPE_MOD:
				return mod(lhs->generate(p), rhs->generate(p));
			default:
				FatalError("Unrecognizeed binop type").raise();
				return (T)0;
		}
	}

	static T mod(const T& a, const T& b) {
		if constexpr (std::is_floating_point_v<T>) return fmod(a, b);
		else return a % b;
	}
private:
	GSBinOpType op;
	GenStep<T>* lhs, * rhs;
};

/*
 * Non-"FAST" types are scaled to cross (0, 0), (2n + 0.5, 1), (2n + 1, 0), and (2n + 1.5, -1)
 * It is advised that one uses the "FAST" versions, which apply no scaling, when convenient
 * Slow versions are good for intuitive but less performmant prototyping
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

template<class T>
class GSOsc : public GenStep<T> {
public:
	GSOsc(GSOscType t, GenStep<T>* a) : type(t), arg(a) {}
	~GSOsc() {
		if (arg) delete arg;
	}

	// TODO: consider rescaling to make defaults work with integral types
	T generate(const std::vector<size_t>& p) const {
		switch (type) {
			case GS_OSC_TYPE_SINE_FAST:
				return sin(arg->generate(p));
			case GS_OSC_TYPE_SAW_FAST:
				return GSBinOp<T>::mod(arg->generate(p), 1); 
			case GS_OSC_TYPE_TRI_FAST:
				return abs(GSBinOp<T>::mod(arg->generate(p), 1) - 0.5); 
			case GS_OSC_TYPE_SQUARE_FAST:
			case GS_OSC_TYPE_SQUARE:
				// TODO: test this with narrowing casts
				// since we're just doing parity hopefully most implementations this will be efficient and work
				if (static_cast<uint8_t>(ceil(arg->generate(p))) % static_cast<uint8_t>(2) == 1) return 1;
				else return 0;
			case GS_OSC_TYPE_SINE:
				return sin(PI * arg->generate(p));
			case GS_OSC_TYPE_SAW:
				return 2 * GSBinOp<T>::mod(arg->generate(p) + 0.5, 1) - 1;
			case GS_OSC_TYPE_TRI:
				return 2 * abs(2 * GSBinOp<T>::mod(arg->generate(p) / 2 - 0.25, 1) - 1) - 1;
			default:
				FatalError("Undefined oscillator type").raise();
				return 0;
		}
	}

private:
	GSOscType type;
	GenStep<T>* arg;
};

typedef enum GSSampleType {
	GS_SAMPLE_TYPE_NEAREST,
	GS_SAMPLE_TYPE_LINEAR
} GSSampleType;

template<class T, class U>
class GSSample : public GenStep<T> {
public:
	GSSample(GSSampleType t, GenStep<T>* s, std::vector<GenStep<U>*> tc) : type(t), src(s), tex_coord(tc) {}

	// TODO: coordinate mirroring/extension
	T generate(const std::vector<size_t>& p) const {
		if (type == GS_SAMPLE_TYPE_NEAREST) {
			std::vector<size_t> nextp(p.size());
			// TODO does this actually round correctly by total distance? 
			for (uint8_t i = 0; i < nextp.size(); i++) {
				nextp[i] = (size_t)round(tex_coord[i]->generate(p));
				if (nextp[i] >= src_width) nextp[i] -= src_width;
				if (nextp[i] < 0) nextp[i] += src_width;
			}
			return src->generate(nextp);
		}
		if (type == GS_SAMPLE_TYPE_LINEAR) {
			std::vector<U> p_loc(p.size());
			for (uint8_t i = 0; i < p.size(); i++) {
				p_loc[i] = tex_coord[i]->generate(p);
			}
			return mix(p.size() - 1, p_loc);
		}
		FatalError("Unrecognized GSSample type").raise();
		return (T)0;
	}

private:
	GSSampleType type;
	GenStep<T>* src;
	size_t src_width = 8;
	std::vector<GenStep<U>*> tex_coord; // in space of source width

	T mix(uint8_t n, const std::vector<U>& p_loc) const {
		U val = p_loc[n];
		U dist = GSBinOp<U>::mod(val, (U)1);
		if (n == 0) {
			std::vector<size_t> p_f(p_loc.size()), p_c(p_loc.size());
			p_f[0] = floorAndClampMin(val, dist);
			p_c[0] = ceilAndClampMax(val, dist);
			for (uint8_t i = 1; i < p_loc.size(); i++) {
				p_f[i] = (size_t)p_loc[i];
				p_c[i] = (size_t)p_loc[i];
			}
			return src->generate(p_f)*(1-dist) + src->generate(p_c)*(dist);
		}
		std::vector<U> p_f(p_loc), p_c(p_loc);
		p_f[n] = floorAndClampMin(val, dist);
		p_c[n] = ceilAndClampMax(val, dist);
		return mix(n-1, p_f)*(1-dist) + mix(n-1, p_c)*(dist);
	}
	U floorAndClampMin(U v, U d) const {
		if (d > v) return v - d + (U)src_width;
		return v - d;
	}
	U ceilAndClampMax(U v, U d) const {
		if (v - d + 1 > (U)src_width) return v - d + 1 - (U) src_width;
		return v - d + 1;
	}
};

template<class T, class U>
class GSLoad : public GenStep<T> {
public:
	GSLoad(const T* d, GenStep<U>* idx) : data(d), index(idx) {}

	T generate(const std::vector<size_t>& p) const {return data[index->generate(p)];}

private:
	const T* data;
	GenStep<U>* index;
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
	void addTexture(std::string n, const std::vector<GenStep<T>*>& roots, size_t res, VkFormat fmt, VkSampler s) {
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
		for (size_t i = 0; i < res; i++) {
			for (size_t j = 0; j < res; j++) {
				for (uint8_t k = 0; k < roots.size(); k++) {
					*scan++ = roots[k]->generate({i, j});
				}
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

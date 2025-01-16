#include <map>
#include <fts.h>

#include <png.h>

#include "GraphicsHandler.h"

// #define VERBOSE_TEXTURESET_OBJECTS

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

private:
	std::map<std::string, ImageInfo> textures;

	/* for move operations, keeps vulkan image objects from being deleted */
	void nukeTextures();
};

class TextureHandler {
public:
	TextureHandler() : defaultsampler(VK_NULL_HANDLE) {}
	~TextureHandler();

	const TextureSet& getSet(std::string n) const {return sets.at(n);}
	VkSampler getSampler(std::string n) {return samplers.at(n);}

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

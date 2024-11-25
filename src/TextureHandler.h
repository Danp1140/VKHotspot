#include <map>
#include <fts.h>

#include <png.h>

#include "GraphicsHandler.h"

class TextureSet {
public:
	TextureSet() = default;
	TextureSet(const char* d);
	~TextureSet();

	const ImageInfo& getDiffuse() const {return diffuse;}
	const ImageInfo& getNormal() const {return normal;}
	void setDiffuseSampler(VkSampler s) {diffuse.sampler = s;}
	void setNormalSampler(VkSampler s) {normal.sampler = s;}

private:
	ImageInfo diffuse, normal;
};

class TextureHandler {
public:
	TextureHandler() = default;
	~TextureHandler();

	VkSampler getSampler(std::string n) {return samplers[n];}

	VkSampler addSampler(
		const char* n, 
		VkFilter mag, VkFilter min, 
		VkSamplerAddressMode addr, 
		VkBool32 aniso);
private:
	std::map<std::string, VkSampler> samplers;
};

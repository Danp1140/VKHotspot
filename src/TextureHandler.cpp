#include "TextureHandler.h"

TextureSet::TextureSet(TextureSet&& rvalue) {
#ifdef VERBOSE_TEXTURESET_OBJECTS
	std::cout << this << " TextureSet(TextureSet&&)" << std::endl;
#endif
	nukeTextures(); // necessary???
	swap(*this, rvalue);
}

TextureSet::TextureSet(const char* d) {
#ifdef VERBOSE_TEXTURESET_OBJECTS
	std::cout << this << " TextureSet(const char*)" << std::endl;
#endif
	char dcpy[strlen(d)];
	strcpy(&dcpy[0], d);
	char* dirptr[2] = {dcpy, nullptr};
	FTS* dir = fts_open(dirptr, FTS_LOGICAL, 0);
	FTSENT* f;
	png_image i;
	png_bytep buffer;
	ImageInfo* dst;
	const char* setname = strrchr(d, '/') + 1;
	size_t setnamelen = strlen(setname);
	while ((f = fts_read(dir))) {
		if (f->fts_level == 1) {
			/*
			if (strcmp(f->fts_name + f->fts_namelen - 11, "diffuse.png") == 0) {
				dst = &diffuse;
				std::cout << f->fts_name << std::endl;
			}
			else if (strcmp(f->fts_name + f->fts_namelen - 10, "normal.png") == 0) {
				dst = &normal;
				std::cout << f->fts_name << std::endl;
			}
			*/
			if (strcmp(f->fts_name + f->fts_namelen - 4, ".png") != 0) {
				WarningError("Unexpected file/directory in TextureSet directory").raise();
				continue;
			}
			else {
				dst = &textures.insert({
						std::string(f->fts_name + setnamelen, f->fts_namelen - setnamelen - 4),
					{}
				}).first->second;
				std::cout << setname << "'s " << std::string(f->fts_name + setnamelen, f->fts_namelen - setnamelen - 4) << std::endl;
			}

			i.version = PNG_IMAGE_VERSION;
			i.opaque = NULL;
			png_image_begin_read_from_file(&i, f->fts_accpath);
			i.format = PNG_FORMAT_RGBA;
			buffer = static_cast<png_bytep>(malloc(PNG_IMAGE_SIZE(i)));

			png_image_finish_read(&i, NULL, buffer, 0, NULL);

			dst->extent = {i.width, i.height};
			// TODO: format setting
			dst->format = VK_FORMAT_R8G8B8A8_SRGB;
			dst->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			dst->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			dst->sampler = VK_NULL_HANDLE;
			GH::createImage(*dst);
			GH::updateImage(*dst, buffer);

			free(buffer);
			png_image_free(&i);
		}
	}
}

TextureSet::~TextureSet() {
#ifdef VERBOSE_TEXTURESET_OBJECTS
	std::cout << this << " ~TextureSet()" << std::endl;
#endif
	for (auto& [_, t] : textures) {
		if (t.image != VK_NULL_HANDLE) GH::destroyImage(t);
	}
}

void swap(TextureSet& lhs, TextureSet& rhs) {
	std::swap(lhs.textures, rhs.textures);
}

TextureSet& TextureSet::operator=(TextureSet rhs) {
#ifdef VERBOSE_TEXTURESET_OBJECTS
	std::cout << this << " = TextureSet" << std::endl;
#endif
	nukeTextures();
	swap(*this, rhs);
	// rhs.nukeTextures();
	return *this;
}

void TextureSet::nukeTextures() {
	for (auto& [_, t] : textures) t = {};
}

TextureHandler::~TextureHandler() {
	for (const std::pair<std::string, VkSampler>& s : samplers) 
		vkDestroySampler(GH::getLD(), s.second, nullptr);
}

void TextureHandler::addSet(std::string n, TextureSet&& t) {
	// TextureSet& newt = sets.insert({n, t}).first->second;
	sets.emplace({n, t});
	// newt.setDiffuseSampler(defaultsampler); // TODO: update to work with generalized TextureSet
}

VkSampler TextureHandler::addSampler(
	const char* n, 
	VkFilter mag, VkFilter min, 
	VkSamplerAddressMode addr, 
	VkBool32 aniso) {
	VkSamplerCreateInfo ci {
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		nullptr,
		0,
		mag, min,
		VK_SAMPLER_MIPMAP_MODE_NEAREST,
		addr, addr, addr,
		0, 
		aniso, aniso ? 16.f : 0.f,
		VK_FALSE, VK_COMPARE_OP_NEVER,
		0, 1,
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
		VK_FALSE
	};
	VkSampler res;
	vkCreateSampler(GH::getLD(), &ci, nullptr, &res);
	samplers.insert({n, res});
	return res;
}

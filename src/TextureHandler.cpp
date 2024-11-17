#include "TextureHandler.h"

TextureSet::TextureSet(const char* d) {
	char dcpy[strlen(d)];
	strcpy(&dcpy[0], d);
	char* dirptr[2] = {dcpy, nullptr};
	FTS* dir = fts_open(dirptr, FTS_LOGICAL, 0);
	FTSENT* f;
	png_image i;
	png_bytep buffer;
	ImageInfo* dst;
	while (f = fts_read(dir)) {
		if (f->fts_level == 1) {	
			if (strcmp(f->fts_name + f->fts_parent->fts_pathlen, "diffuse")) {
				dst = &diffuse;
			}
			else {
				WarningError("Unexpected file in TextureSet directory").raise();
				continue;
			}

			i.version = PNG_IMAGE_VERSION;	
			i.opaque = NULL;
			png_image_begin_read_from_file(&i, f->fts_accpath);
			i.format = PNG_FORMAT_RGBA;
			buffer = static_cast<png_bytep>(malloc(PNG_IMAGE_SIZE(i)));

			png_image_finish_read(&i, NULL, buffer, 0, NULL);

			dst->extent = {i.width, i.height};
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
	if (diffuse.image != VK_NULL_HANDLE) GH::destroyImage(diffuse);
}

TextureHandler::~TextureHandler() {
	for (const std::pair<const char*, VkSampler>& s : samplers) 
		vkDestroySampler(GH::getLD(), s.second, nullptr);
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

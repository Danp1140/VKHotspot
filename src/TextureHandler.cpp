#include "TextureHandler.h"

/*
void OSBrownian::generate(void* dst, VkExtent2D ext) {
	GSLoadData loads[depth];
	GSSample upsamples[depth];
	GSMult normalize(1 / depth);
	GSCoordX x(ext.width);
	GSCoordY y(ext.width);

	void* levels[depth];
	for (uint8_t i = 0; i < depth; i++) {
		VkExtent2D e = {1 << i, 1 << i};
		levels[i] = malloc(pow(1 << i, 2));
		Generator::noise(levels[i], e);
		loads[i] = GSLoadData(levels[i]);
		upsamples[i] = GSSample(ext.width / e.width);
	}

	
	StepNode tree[4 * depth + 1];
	tree[0].gs = x;
	tree[1].gs = y;

	tree[2].ins.push_back(&tree[0]);
	tree[2].ins.push_back(&tree[1]);
	tree[2].gs = loads[0];

	tree[3].ins.push_back(&tree[2]);
	tree[3].gs = upsamples[0];

	tree[4].ins.push_back(&tree[3]);
	tree[4].gs = normalize;

	StepNode* lasttop = &tree[2];

	const size_t idxoffset = 5;

	for (uint8_t i = 1; i < depth; i++) {
		tree[(i - 1) * 4 + idxoffset].ins.push_back(&tree[0]);
		tree[(i - 1) * 4 + idxoffset].ins.push_back(&tree[1]);
		tree[(i - 1) * 4 + idxoffset].gs = loads[i];

		tree[(i - 1) * 4 + idxoffset + 1].ins.push_back{&tree[(i - 1) * 4 + 3]};
		tree[(i - 1) * 4 + idxoffset + 1].gs = upsamples[i];

		tree[(i - 1) * 4 + idxoffset + 2].ins.push_back(&tree[(i - 1) * 4 + 4]);
		tree[(i - 1) * 4 + idxoffset + 2].gs = normalize;

		tree[(i - 1) * 4 + idxoffset + 3].ins_push_back(&tree[(i - 1) * 4 + 5]);
	}
	*/

	/*
	 * load base level
	 * upscale to dst
	 * divide by num levels
	 * set to dst
	 *
	 * for each next level
	 * load level
	 * upscale to dst
	 * divide by num levels
	 * add to dst
	 */

/*
	for (uint8_t i = 0; i < depth; i++) {
		free(levels[i]);
	}
}
*/

/*
char GSAdd::generate(char* in) const {
	return in[0] + in[1];
}

char GSMult::generate(char* in) const {
	return in[0] * in[1];
}

char GSLoadData::generate(char* in) const {
	return ((char*)data)[elemsize * (width * in[0] + in[1])];
}

char GSCoordX::generate(char* in) const {
	return in[0] % width;
}

char GSCoordY::generate(char* in) const {
	return in[0] / width;
}

void Generator::generate(void* dst, VkExtent2D ext, size_t esize) {
	char* buffer = (char*)dst;

	for (uint32_t i = 0; i < ext.width; i++) {
		for (uint32_t j = 0; j < ext.height; j++) {
			buffer[i * ext.height + j] = roots[0].generate({i, j}, esize); // TODO: change to work across all roots
		}
	}
}
*/

/*
void Generator::noise(void* dst, VkExtent2D ext) {
	std::random_device rdev;
	std::default_random_engine reng(rdev());
	std::uniform_int_distribution<uint8_t> unidist();
	char* dstscan = dst;
	for (size_t i = 0; i < ext.width * ext.height; i++) {
		*dstscan++ = unidist(reng);
	}
}
*/

TextureSet::TextureSet(TextureSet&& rvalue) : textures(std::move(rvalue.textures)) {
#ifdef VERBOSE_TEXTURESET_OBJECTS
	std::cout << this << " TextureSet(TextureSet&&)" << std::endl;
#endif
	rvalue.nukeTextures(); // necessary???
}

TextureSet::TextureSet(const char* d, VkSampler s) {
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
				// std::cout << setname << "'s " << std::string(f->fts_name + setnamelen, f->fts_namelen - setnamelen - 4) << std::endl;
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
			dst->sampler = s;
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

TextureSet& TextureSet::operator=(TextureSet&& rhs) {
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
	sets.emplace(n, std::move(t));
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

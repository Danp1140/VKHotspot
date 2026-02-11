#include "Scene.h"
#include "TextureHandler.h"

RenderPassInfo getRP(const WindowInfo& w) {
	VkRenderPass r;
	VkAttachmentDescription attachdescs[2] {{
			0, 
			GH_SWAPCHAIN_IMAGE_FORMAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		}, {
			0, 
			GH_DEPTH_BUFFER_IMAGE_FORMAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
	}};
	VkAttachmentReference attachrefs[2] {
		{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
		{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}
	};
	GH::createRenderPass(r, 2, &attachdescs[0], &attachrefs[0], nullptr, &attachrefs[1]);

	return RenderPassInfo(r, w.getNumSCIs(), w.getSCImages(), nullptr, w.getDepthBuffer(), {{0.3, 0.3, 0.3, 1}, {1, 0}});
}

void createDPipeline(PipelineInfo& p, const WindowInfo& w, VkRenderPass r) {
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "d";
	VkDescriptorSetLayoutBinding dtbindings {
		0, // diffuse
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	};
	p.descsetlayoutci = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1, &dtbindings
	};
	p.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePCData)};
	p.objpushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, sizeof(ScenePCData), sizeof(MeshPCData)};
	p.vertexinputstateci = Mesh::getVISCI(VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL);
	p.depthtest = true;
	p.extent = w.getSCExtent();
	p.renderpass = r;
	p.msaasamples = w.getMSAASamples();
	GH::createPipeline(p);
	Mesh::ungetVISCI(p.vertexinputstateci); // TODO: can we do this before addPipeline???
}

void createDNSPipeline(PipelineInfo& p, const WindowInfo& w, VkRenderPass r) {
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "dns";
	VkDescriptorSetLayoutBinding dtbindings[3] {{
		0, // diffuse
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	}, {
		1, // normal
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	}, {
		2, // specular
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	}};
	p.descsetlayoutci = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		3, &dtbindings[0]
	};
	p.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePCData)};
	p.objpushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, sizeof(ScenePCData), sizeof(MeshPCData)};
	p.vertexinputstateci = Mesh::getVISCI(VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL);
	p.depthtest = true;
	p.extent = w.getSCExtent();
	p.renderpass = r;
	p.msaasamples = w.getMSAASamples();
	GH::createPipeline(p);
	Mesh::ungetVISCI(p.vertexinputstateci); // TODO: can we do this before addPipeline???
}

void createSolidTex(TextureSet& ts, size_t res, VkSampler s) {
	GSConst<uint8_t>* r = new GSConst((uint8_t)0xff), 
		* g = new GSConst((uint8_t)0xdd), 
		* b = new GSConst((uint8_t)0x11), 
		* a = new GSConst((uint8_t)0xff);
	std::vector<GenStep<uint8_t>*> roots = {r, g, b, a};

	ts.addTexture("diffuse", roots, res, VK_FORMAT_R8G8B8A8_UNORM, s);

	for (GenStep<uint8_t>* r : roots) delete r;
}

void createPlaidTex(TextureSet& ts, size_t res, VkSampler s) {
	GSConst<float>* green = new GSConst(0.f), 
		* blue = new GSConst(0.f), 
		* alpha = new GSConst(1.f), 
		* offset_c = new GSConst(2.f), 
		* norm_c = new GSConst(0.25f);
	GSDim* xcoord = new GSDim(0);
	GSDim* ycoord = new GSDim(1);
	GSCast<float, size_t>* xcoord_f = new GSCast<float, size_t>(xcoord);
	GSCast<float, size_t>* ycoord_f = new GSCast<float, size_t>(ycoord);
	GSOsc<float>* sine1 = new GSOsc(GS_OSC_TYPE_SINE_FAST, xcoord_f);
	GSOsc<float>* sine2 = new GSOsc(GS_OSC_TYPE_SINE_FAST, ycoord_f);
	GSBinOp<float>* comb = new GSBinOp(GS_BINOP_TYPE_ADD, sine1, sine2);
	GSBinOp<float>* offset = new GSBinOp(GS_BINOP_TYPE_ADD, comb, offset_c);
	GSBinOp<float>* normalize = new GSBinOp(GS_BINOP_TYPE_MULT, offset, norm_c);
	std::vector<GenStep<float>*> roots = {normalize, green, blue, alpha};

	ts.addTexture("diffuse", roots, res, VK_FORMAT_R32G32B32A32_SFLOAT, s);
	for (GenStep<float>* r : roots) delete r;
}

void createNoiseTex(TextureSet& ts, size_t res, VkSampler s) {
	NDSNoise<uint32_t> n;

	uint32_t* tmp = new uint32_t[res * res];
	for (size_t i = 0; i < res * res; i++) tmp[i] = n.generate(nullptr);

	GSDim* xcoord = new GSDim(0);
	GSDim* ycoord = new GSDim(1);
	GSConst<size_t>* res_c = new GSConst(res);
	GSBinOp<size_t>* idx_tmp = new GSBinOp(GS_BINOP_TYPE_MULT, xcoord, res_c);
	GSBinOp<size_t>* idx = new GSBinOp(GS_BINOP_TYPE_ADD, idx_tmp, ycoord);
	GSLoad<uint32_t, size_t>* l = new GSLoad(tmp, idx);
	GSConst<uint8_t>* c = new GSConst((uint8_t)0xff);
	GSCast<uint8_t, uint32_t>* cast = new GSCast<uint8_t, uint32_t>(l);

	std::vector<GenStep<uint8_t>*> roots = {cast, cast, cast, c};

	ts.addTexture("diffuse", roots, res, VK_FORMAT_R8G8B8A8_UNORM, s);

	delete cast;
	delete c;
	delete[] tmp;
}

void createGradTex(TextureSet& ts, size_t res, VkSampler s) {
	GSConst<float>* c = new GSConst(1.f);

	GSDim* x = new GSDim(0);
	GSDim* y = new GSDim(1);
	GSConst<float>* norm_x = new GSConst(1.f / (float)res);
	GSConst<float>* norm_y = new GSConst(1.f / (float)res);
	GSCast<float, size_t>* x_f = new GSCast<float, size_t>(x);
	GSCast<float, size_t>* y_f = new GSCast<float, size_t>(y);
	GSBinOp<float>* x_norm = new GSBinOp(GS_BINOP_TYPE_MULT, x_f, norm_x);
	GSBinOp<float>* y_norm = new GSBinOp(GS_BINOP_TYPE_MULT, y_f, norm_y);

	std::vector<GenStep<float>*> roots = {c, y_norm, x_norm, c};

	ts.addTexture("diffuse", roots, res, VK_FORMAT_R32G32B32A32_SFLOAT, s);

	delete x_norm;
	delete y_norm;
	delete c;
}

void createBrownianTex(TextureSet& ts, size_t res, VkSampler s) {
	std::cout << "started gen" << std::endl;
	NDSNoise<uint8_t> n;
	uint8_t* tmp = new uint8_t[8*8];
	for (size_t i = 0; i < 8*8; i++) tmp[i] = n.generate(nullptr);

	GSConst<uint8_t>* a = new GSConst((uint8_t)0x00);
	

	GSDim* xcoord = new GSDim(0);
	GSDim* ycoord = new GSDim(1);
	GSCast<float, size_t>* f_x = new GSCast<float, size_t>(xcoord);
	GSCast<float, size_t>* f_y = new GSCast<float, size_t>(ycoord);
	GSConst<float>* ds_c = new GSConst(8.f/(float)res);
	GSBinOp<float>* ds_x = new GSBinOp(GS_BINOP_TYPE_MULT, f_x, ds_c);
	GSBinOp<float>* ds_y = new GSBinOp(GS_BINOP_TYPE_MULT, f_y, ds_c);

	GSDim* l_x = new GSDim(0);
	GSDim* l_y = new GSDim(1);
	GSConst<size_t>* l_c = new GSConst((size_t)8);
	GSBinOp<size_t>* l_m = new GSBinOp(GS_BINOP_TYPE_MULT, l_x, l_c);
	GSBinOp<size_t>* l_idx = new GSBinOp(GS_BINOP_TYPE_ADD, l_m, l_y);
	GSLoad<uint8_t, size_t>* l = new GSLoad(tmp, l_idx);
	
	GSSample<uint8_t, float>* samp = new GSSample<uint8_t, float>(GS_SAMPLE_TYPE_LINEAR, l, {ds_x, ds_y});
	// GSSample<uint8_t, float>* samp = new GSSample<uint8_t, float>(GS_SAMPLE_TYPE_NEAREST, l, {ds_x, ds_y});

	std::vector<GenStep<uint8_t>*> roots = {samp, samp, samp, a};

	ts.addTexture("diffuse", roots, res, VK_FORMAT_R8G8B8A8_UNORM, s);

	delete samp;
	delete a;
	delete[] tmp;

	std::cout << "ended gen" << std::endl;
}

int main() {
	GH gh;
	WindowInfo w;
	Scene s((float)w.getSCExtent().width / (float)w.getSCExtent().height);
	s.getCamera()->setPos(glm::vec3(15, 6, 15));
	s.getCamera()->setForward(glm::vec3(-15, -6, -15));
	RenderPassInfo rp = getRP(w);
	PipelineInfo dpipeline;
	createDPipeline(dpipeline, w, rp.getRenderPass());
	rp.addPipeline(dpipeline, &s.getCamera()->getVP());
	// TODO could unget visci here if needed
	s.addRenderPass(rp);

	TextureHandler th;
	th.setDefaultSampler(th.addSampler("linminmag", VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_FALSE));
	TextureSet solid, plaid, noise, grad, brow;
	/*
	createSolidTex(solid, 1, th.getDefaultSampler());
	createPlaidTex(plaid, 512, th.getDefaultSampler());
	createNoiseTex(noise, 512, th.getDefaultSampler());
	*/
	createGradTex(grad, 512, th.getDefaultSampler());
	createBrownianTex(brow, 512, th.getDefaultSampler());
	/*
	th.addSet("solid", std::move(solid));
	th.addSet("plaid", std::move(plaid));
	th.addSet("noise", std::move(noise));
	*/
	th.addSet("grad", std::move(grad));
	th.addSet("brow", std::move(brow));

	Mesh plane("../resources/models/plane.obj");
	Mesh plane2("../resources/models/plane.obj");
	plane2.setPos(plane2.getPos() + glm::vec3(0, 0, -20));

	VkDescriptorSet temp;
	GH::createDS(s.getRenderPass(0).getRenderSet(0).pipeline, temp);
	GH::updateDS(temp, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, th.getSet("brow").getTexture("diffuse").getDII(), {});
	s.getRenderPass(0).addMesh(&plane, temp, &plane.getModelMatrix(), 0);
	s.getRenderPass(0).addMesh(&plane2, temp, &plane2.getModelMatrix(), 0);

	w.addTasks(s.getDrawTasks());

	while (w.frameCallback()) {
		SDL_PumpEvents();
	}

	vkQueueWaitIdle(GH::getGenericQueue());

	return 0;
}

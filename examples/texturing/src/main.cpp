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
	GSConst<char> r(0xff), g(0xdd), b(0x11), a(0xff);

	StepNode<char> r_root({}, &r), g_root({}, &g), b_root({}, &b), a_root({}, &a);
	std::vector<GenStepNode<char>*> roots = {&r_root, &g_root, &b_root, &a_root};

	ts.addTexture("diffuse", roots, res, VK_FORMAT_R8G8B8A8_UNORM, s);
}

void createPlaidTex(TextureSet& ts, size_t res, VkSampler s) {
	GSConst<float> c1(0), c2(1), norm(1 / (float)res), c3((float)res), c4(-1 / (float)res), c5(2), c6(0.25);
	GSOsc<float> sinwave(GS_OSC_TYPE_SINE_FAST);
	GSAdd<float> add;
	GSMultiply<float> mult;
	GSModu<float> modu;

	StepNode<float> y_coord({(GenStepNode<float>*)SN_PIXEL_ID, (GenStepNode<float>*)&c3}, &modu);
	StepNode<float> x_coord_0({(GenStepNode<float>*)SN_PIXEL_ID, (GenStepNode<float>*)&norm}, &mult);
	StepNode<float> x_coord_1({(GenStepNode<float>*)&y_coord, (GenStepNode<float>*)&c4}, &mult);
	StepNode<float> x_coord({(GenStepNode<float>*)&x_coord_0, (GenStepNode<float>*)&x_coord_1}, &add);
	StepNode<float> sine1({(GenStepNode<float>*)&x_coord}, &sinwave);
	StepNode<float> sine2({(GenStepNode<float>*)&y_coord}, &sinwave);
	StepNode<float> comb({(GenStepNode<float>*)&sine1, (GenStepNode<float>*)&sine2}, &add);
	StepNode<float> offset({(GenStepNode<float>*)&comb, (GenStepNode<float>*)&c5}, &add);
	StepNode<float> r_root({(GenStepNode<float>*)&offset, (GenStepNode<float>*)&c6}, &mult), g_root({}, &c1), b_root({}, &c1), a_root({}, &c2);
	std::vector<GenStepNode<float>*> roots = {&r_root, &g_root, &b_root, &a_root};

	ts.addTexture("diffuse", roots, res, VK_FORMAT_R32G32B32A32_SFLOAT, s);
}

void createNoiseTex(TextureSet& ts, size_t res, VkSampler s) {
	NDSNoise<uint32_t> n;

	uint32_t* tmp = new uint32_t[res * res];
	for (size_t i = 0; i < res * res; i++) tmp[i] = n.generate(nullptr);

	GSLoad<uint32_t> l(tmp);
	StepNode<uint32_t> lstep({SN_PIXEL_ID}, &l);
	GSConst<uint8_t> c(0xff);
	CastStepNode<uint8_t, uint32_t> cast(&lstep);

	StepNode<uint8_t> a_root({}, &c);
	std::vector<GenStepNode<uint8_t>*> roots = {&cast, &cast, &cast, &a_root};

	ts.addTexture("diffuse", roots, res, VK_FORMAT_R8G8B8A8_UNORM, s);

	delete[] tmp;
}

void createGradTex(TextureSet& ts, size_t res, VkSampler s) {
	GSConst<float> c(1);

	CastStepNode<float, uint32_t> cast(SN_PIXEL_ID);

	StepNode<float> a_root({}, &c);
	std::vector<GenStepNode<float>*> roots = {&cast, &cast, &cast, &a_root};

	ts.addTexture("diffuse", roots, res, VK_FORMAT_R32G32B32A32_SFLOAT, s);
}

void createBrownianTex(TextureSet& ts, size_t res, VkSampler s) {
	NDSNoise<float> n(0, 1);
	float* tmp = new float[2*2];
	for (size_t i = 0; i < res * res; i++) tmp[i] = n.generate(nullptr);

	StepNode<float> 

	GSConst<float> a_const(1);
	StepNode<float> a_root({}, &a_const);
	std::vector<GenStepNode<float>*> roots = {&a_root};

	ts.addTexture("diffuse", roots, res, VK_FORMAT_R32G32B32A32_SFLOAT, s);
	delete[] tmp;
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
	TextureSet solid, plaid, noise, grad;
	createSolidTex(solid, 1, th.getDefaultSampler());
	createPlaidTex(plaid, 512, th.getDefaultSampler());
	createNoiseTex(noise, 512, th.getDefaultSampler());
	createGradTex(grad, 512, th.getDefaultSampler());
	th.addSet("solid", std::move(solid));
	th.addSet("plaid", std::move(plaid));
	th.addSet("noise", std::move(noise));
	th.addSet("grad", std::move(grad));

	Mesh plane("../resources/models/plane.obj");

	VkDescriptorSet temp;
	GH::createDS(s.getRenderPass(0).getRenderSet(0).pipeline, temp);
	GH::updateDS(temp, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, th.getSet("plaid").getTexture("diffuse").getDII(), {});
	s.getRenderPass(0).addMesh(&plane, temp, &plane.getModelMatrix(), 0);

	w.addTasks(s.getDrawTasks());

	while (w.frameCallback()) {
		SDL_PumpEvents();
	}

	vkQueueWaitIdle(GH::getGenericQueue());

	return 0;
}

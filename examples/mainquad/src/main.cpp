#include "Scene.h"

Scene createScene(const WindowInfo& w);

int main() {
	GH graphicshandler;
	WindowInfo w;

	Scene s = createScene(w);

	s.getCamera()->setPos(glm::vec3(100));
	s.getCamera()->setForward(glm::vec3(-1));

	Mesh floor("../resources/models/floor.obj");
	s.getRenderPass(0).addMesh(&floor, VK_NULL_HANDLE, &floor.getModelMatrix(), 0);

	std::vector<InstancedMeshData> imdtemp(8);
	imdtemp[0].m = glm::translate(glm::mat4(1), glm::vec3(-13.8278, 0, -21.9972));
	imdtemp[1].m = glm::translate(imdtemp[0].m, glm::vec3(0, 0, 43.79));
	for (uint8_t i = 0; i < 2; i++) {
		// imdtemp[2 + i].m = glm::scale(imdtemp[i].m, glm::vec3(-1, 1, 1));
		imdtemp[2 + i].m = imdtemp[i].m;
		imdtemp[2 + i].m[0][0] *= -1;
	}
	for (uint8_t i = 0; i < 4; i++) {
		imdtemp[4 + i].m = glm::scale(imdtemp[i].m, glm::vec3(1, 1, -1));
		imdtemp[4 + i].m = imdtemp[i].m;
		imdtemp[4 + i].m[1][1] *= -1;
	}
	InstancedMesh largeplanters("../resources/models/largeplanter.obj", imdtemp);
	VkDescriptorSet dstemp;
	GH::createDS(s.getRenderPass(0).getRenderSet(1).pipeline, dstemp);
	GH::updateDS(dstemp, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {}, largeplanters.getInstanceUB().getDBI());
	s.getRenderPass(0).addMesh(&largeplanters, dstemp, nullptr, 1);
	
	w.addTasks(s.getDrawTasks());

	while (w.frameCallback()) {}

	vkQueueWaitIdle(GH::getGenericQueue());

	return 0;
}

Scene createScene(const WindowInfo& w) {
	Scene s((float)w.getSCExtent().width / (float)w.getSCExtent().height);

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
	GH::createRenderPass(r, 2, &attachdescs[0], &attachrefs[0], &attachrefs[1]);

	RenderPassInfo primaryrp(
		r, 
		w.getNumSCIs(), 
		w.getSCImages(), 
		w.getDepthBuffer(), 
		{{0.1, 0.1, 0.1, 0.1}, {1, 0}});

	PipelineInfo p;
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "viewport";
	p.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePCData) + sizeof(MeshPCData)};
	p.vertexinputstateci = Mesh::getVISCI(
		VERTEX_BUFFER_TRAIT_POSITION 
		 | VERTEX_BUFFER_TRAIT_UV 
		 | VERTEX_BUFFER_TRAIT_NORMAL);
	p.depthtest = true;
	p.extent = w.getSCExtent();
	p.renderpass = r;
	GH::createPipeline(p);
	primaryrp.addPipeline(p, &s.getCamera()->getVP());

	p.shaderfilepathprefix = "viewportinstanced";
	VkDescriptorSetLayoutBinding bindings[1] {{
		0,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		1,
		VK_SHADER_STAGE_VERTEX_BIT,
		nullptr
	}};
	p.descsetlayoutci = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1, &bindings[0]
	};
	GH::createPipeline(p);
	primaryrp.addPipeline(p, &s.getCamera()->getVP());
	Mesh::ungetVISCI(p.vertexinputstateci);

	s.addRenderPass(primaryrp);

	return s;
}


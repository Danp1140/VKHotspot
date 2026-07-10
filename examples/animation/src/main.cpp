#include <fbxsdk.h>
#include <iostream>
#include <Scene.h>
#include <TextureHandler.h>

#define VB_TRAIT_ALL (VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL | VERTEX_BUFFER_TRAIT_TANGENT | VERTEX_BUFFER_TRAIT_BITANGENT)

typedef struct [[gnu::packed]] DNSScenePCData {
	glm::mat4 vp;
	glm::vec3 c_pos;
} DNSScenePCData;

typedef struct [[gnu::packed]] DNSObjectPCData {
	uint32_t catcher_idx;
	glm::mat4 m;
} DNSObjectPCData;

RenderPassInfo* createMainRenderPass(Scene& s, WindowInfo& w) {
	VkRenderPass r;
	VkAttachmentDescription a_d[3] {{
			0, 
			GH_SWAPCHAIN_IMAGE_FORMAT,
			w.getMSAASamples(),
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		}, {
			0, 
			GH_DEPTH_BUFFER_IMAGE_FORMAT,
			w.getMSAASamples(),
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		}, {
			0, 
			GH_SWAPCHAIN_IMAGE_FORMAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	}};
	VkAttachmentReference a_r[3] {
		{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
		{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
		{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
	};
	GH::createRenderPass(r, 3, &a_d[0], &a_r[0], &a_r[2], &a_r[1]);
	std::vector<const ImageInfo*> a_imgs = {&w.getMSAAImage(), w.getDepthBuffer(), w.getSCImages()};
	RenderPassInfo rpi(r, w.getNumSCIs(), w.getMSAAImage().extent, {{0.3, 0.3, 0.3, 1}, {1, 0}}, a_imgs, 2);

	return s.addRenderPass(rpi);
}

size_t createDNSPipeline(RenderPassInfo& rpi, Scene& s, const WindowInfo& w) {
	PipelineInfo p;
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "dns";
	VkDescriptorSetLayoutBinding dtbindings[6] {{
			0,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			1,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		}, {
			1,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		}, {
			2,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			1,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		}, {
			3,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		}, {
			4,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		}, {
			5,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
	}};
	p.descsetlayoutci = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		6, &dtbindings[0]
	};
	p.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DNSScenePCData)};
	p.objpushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(DNSScenePCData), sizeof(DNSObjectPCData)};
	p.vertexinputstateci = Mesh::getVISCI(
			VERTEX_BUFFER_TRAIT_POSITION
			| VERTEX_BUFFER_TRAIT_UV 
			| VERTEX_BUFFER_TRAIT_NORMAL
			| VERTEX_BUFFER_TRAIT_TANGENT
			| VERTEX_BUFFER_TRAIT_BITANGENT);
	p.depthtest = true;
	p.extent = w.getSCExtent();
	p.renderpass = rpi.getRenderPass();
	p.msaasamples = w.getMSAASamples();
	GH::createPipeline(p);
	Mesh::ungetVISCI(p.vertexinputstateci);
	return rpi.addPipeline(p, nullptr);
}

size_t createViewportPipeline(RenderPassInfo& rpi, Scene& s, const WindowInfo& w) {
	PipelineInfo p;
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "viewport";
	p.pushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::mat4)};
	p.objpushconstantrange = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(glm::mat4)};
	p.vertexinputstateci = Mesh::getVISCI(
			VERTEX_BUFFER_TRAIT_POSITION
			| VERTEX_BUFFER_TRAIT_UV 
			| VERTEX_BUFFER_TRAIT_NORMAL);
	p.depthtest = true;
	p.extent = w.getSCExtent();
	p.renderpass = rpi.getRenderPass();
	p.msaasamples = w.getMSAASamples();
	GH::createPipeline(p);
	Mesh::ungetVISCI(p.vertexinputstateci);
	return rpi.addPipeline(p, nullptr);
}

void printNode(FbxNode* node) {
	int default_attrib_idx = node->GetDefaultNodeAttributeIndex();
	if (default_attrib_idx != -1) {
		FbxNodeAttribute::EType default_attrib = node->GetNodeAttributeByIndex(default_attrib_idx)->GetAttributeType();
		if (default_attrib == FbxNodeAttribute::eMesh) {
			std::cout << "mesh!" << std::endl;
			FbxMesh* mesh = node->GetMesh();
			int poly_count = mesh->GetPolygonCount();
			std::cout << poly_count << " polys" << std::endl;
			/*
			for (int i = 0; i < poly_count; i++) {
				int size = mesh->GetPolygonSize(i);
				std::cout << size << "-gon" << std::endl;
			}
			*/
			int n_deformers = mesh->GetDeformerCount();
			std::cout << n_deformers << " deformers" << std::endl;
			for (int i = 0; i < n_deformers; i++) {
				if (mesh->GetDeformer(i)->GetDeformerType() == FbxBlendShape::EDeformerType::eSkin) {
					FbxSkin* skin = dynamic_cast<FbxSkin*>(mesh->GetDeformer(i));
					int n_clusters = skin->GetClusterCount();
					std::cout << n_clusters << " clusters" << std::endl;
					for (int j = 0; j < n_clusters; j++) {
						FbxCluster* cluster = skin->GetCluster(j);
						std::cout << "link addr: " << cluster->GetLink() << std::endl;
						int n_weights = cluster->GetControlPointIndicesCount();
						std::cout << n_weights << " weights:" << std::endl;
						/*
						double* weights = cluster->GetControlPointWeights();
						for (int k = 0; k < n_weights; k++) {
							std::cout << weights[k] << std::endl;
						}
						*/
					}
				}
			}
		}
		else if (default_attrib == FbxNodeAttribute::eSkeleton)
			std::cout << "skele!" << std::endl;
			std::cout << "addr: " << node << std::endl;
	}
	else std::cout << "not mesh or skele" << std::endl;
	for (int i = 0; i < node->GetChildCount(); i++) {
		printNode(node->GetChild(i));
	}
}

int findFirstMesh(FbxScene* s) {
	for (int n_i = 0; n_i < s->GetNodeCount(); n_i++) {
		FbxNode* n = s->GetNode(n_i);
		int default_attrib_idx = n->GetDefaultNodeAttributeIndex();
		if (default_attrib_idx != -1 && n->GetNodeAttributeByIndex(default_attrib_idx)->GetAttributeType() == FbxNodeAttribute::eMesh) return n_i;
	}
	return -1;
}

/*
 * Presumes given node is a mesh
 * Also presumes it is triangulated (may triangulate it in future)
 * and only deformer is the skin
 */
Mesh meshFromNode(FbxNode* node) {
	FbxMesh* mesh = node->GetMesh();
	// FbxSkin* skin = dynamic_cast<FbxSkin*>(mesh->GetDeformer(0));
	// int n_bones = skin->GetClusterCount();
	size_t n_polys = (size_t)mesh->GetPolygonCount();
	Mesh result = Mesh(VERTEX_BUFFER_TRAIT_POSITION | VERTEX_BUFFER_TRAIT_UV | VERTEX_BUFFER_TRAIT_NORMAL, n_polys*3*(3+2+3)*sizeof(float), n_polys * 3 * sizeof(MeshIndex), 0);


	int uv_layer_idx = mesh->GetLayerIndex(0, FbxLayerElement::EType::eUV, false);
	FbxLayerElementUV* uvs = mesh->GetLayer(uv_layer_idx)->GetUVs();
	if (uvs->GetMappingMode() == fbxsdk::FbxLayerElement::EMappingMode::eByPolygonVertex) {
		std::cout << "uv mapping scheme by polygon vert!\n";
	}
	else FatalError("Unsupported UV mapping mode").raise();
	if (uvs->GetReferenceMode() == fbxsdk::FbxLayerElement::EReferenceMode::eIndexToDirect) {
		std::cout << "uv ref scheme index to direct!\n";
	}
	else FatalError("Unsupported UV reference mode").raise();

	int norm_layer_idx = mesh->GetLayerIndex(0, FbxLayerElement::EType::eNormal, false);
	FbxLayerElementNormal* norms = mesh->GetLayer(norm_layer_idx)->GetNormals();
	FbxLayerElementArrayTemplate<FbxVector4>& norm_arr = norms->GetDirectArray();
	if (norms->GetMappingMode() == fbxsdk::FbxLayerElement::EMappingMode::eByControlPoint) std::cout << "norm mapping scheme by cp!\n";
	if (norms->GetMappingMode() == fbxsdk::FbxLayerElement::EMappingMode::eByPolygonVertex) std::cout << "norm mapping scheme by polygon vert!\n";
	if (norms->GetReferenceMode() == fbxsdk::FbxLayerElement::EReferenceMode::eDirect) std::cout << "norm ref scheme direct!\n";
	if (norms->GetReferenceMode() == fbxsdk::FbxLayerElement::EReferenceMode::eIndex) std::cout << "norm ref scheme index!\n";
	if (norms->GetReferenceMode() == fbxsdk::FbxLayerElement::EReferenceMode::eIndexToDirect) std::cout << "norm ref scheme index to direct!\n";

	float* vertices = new float[n_polys*3*(3+2+3)];
	MeshIndex* indices = new MeshIndex[n_polys*3];

	for (size_t poly_i = 0; poly_i < n_polys; poly_i++) {
		indices[poly_i] = poly_i;
		for (uint8_t vert_i = 0; vert_i < 3; vert_i++) {
			FbxVector4 cp = mesh->GetControlPoints()[mesh->GetPolygonVertex(poly_i, vert_i)];
			vertices[poly_i + 3*vert_i] = cp[0];
			vertices[poly_i + 3*vert_i+1] = cp[1];
			vertices[poly_i + 3*vert_i+2] = cp[2];
			if (uvs->GetReferenceMode() == fbxsdk::FbxLayerElement::EReferenceMode::eIndexToDirect) {
				FbxVector2 uv;
				bool unmapped;
				mesh->GetPolygonVertexUV(poly_i, vert_i, uvs->GetName(), uv, unmapped);
				if (unmapped) {
					vertices[poly_i + 3*vert_i+3] = 0;
					vertices[poly_i + 3*vert_i+4] = 0;
				}
				else {
					vertices[poly_i + 3*vert_i+3] = uv[0];
					vertices[poly_i + 3*vert_i+4] = uv[1];
				}
			}
			if (uvs->GetReferenceMode() == fbxsdk::FbxLayerElement::EReferenceMode::eDirect) {
				FbxVector4 norm;
				mesh->GetPolygonVertexNormal(poly_i, vert_i, norm);
				vertices[poly_i + 3*vert_i+5] = norm[0];
				vertices[poly_i + 3*vert_i+6] = norm[1];
				vertices[poly_i + 3*vert_i+7] = norm[2];
			}
		}
	}

	GH::updateWholeBuffer(result.getVertexBuffer(), vertices);
	GH::updateWholeBuffer(result.getIndexBuffer(), indices);

	/* 
	 * make raw per-polyvert list
	 * test it
	 * when it works, run a pass that checks for uniqueness and sets indices/removes items if not unique (if scrolling, can use an accumulating offset to allow shifting the rest down upon removal)
	 */

	/*
	std::vector<FbxVector2> uv_vec(n_verts);

	int norm_layer_idx = mesh->GetLayerIndex(0, FbxLayerElement::EType::eNormal, false);
	FbxLayerElementNormal* norms = mesh->GetLayer(norm_layer_idx)->GetNormals();
	norms->SetMappingMode(fbxsdk::FbxLayerElement::EMappingMode::eByControlPoint);
	norms->SetReferenceMode(fbxsdk::FbxLayerElement::EReferenceMode::eDirect);
	FbxLayerElementArrayTemplate<FbxVector4>& norm_arr = norms->GetDirectArray();
	if (norms->GetMappingMode() == fbxsdk::FbxLayerElement::EMappingMode::eByControlPoint) std::cout << "norm mapping scheme by cp!\n";
	if (norms->GetMappingMode() == fbxsdk::FbxLayerElement::EMappingMode::eByPolygonVertex) std::cout << "norm mapping scheme by polygon vert!\n";
	if (norms->GetReferenceMode() == fbxsdk::FbxLayerElement::EReferenceMode::eDirect) std::cout << "norm ref scheme direct!\n";
	if (norms->GetReferenceMode() == fbxsdk::FbxLayerElement::EReferenceMode::eIndex) std::cout << "norm ref scheme index!\n";
	if (norms->GetReferenceMode() == fbxsdk::FbxLayerElement::EReferenceMode::eIndexToDirect) std::cout << "norm ref scheme index to direct!\n";
	*/



	/*
	mesh->GenerateTangentsData(uv_layer_idx, true);

	int tan_layer_idx = mesh->GetLayerIndex(0, FbxLayerElement::EType::eTangent, false);
	FbxLayerElementTangent* tans = mesh->GetLayer(tan_layer_idx)->GetTangents();
	tans->SetMappingMode(fbxsdk::FbxLayerElement::EMappingMode::eByControlPoint);
	tans->SetReferenceMode(fbxsdk::FbxLayerElement::EReferenceMode::eDirect);
	FbxLayerElementArrayTemplate<FbxVector4>& tan_arr = tans->GetDirectArray();

	int bitan_layer_idx = mesh->GetLayerIndex(0, FbxLayerElement::EType::eBiNormal, false);
	FbxLayerElementBinormal* bitans = mesh->GetLayer(bitan_layer_idx)->GetBinormals();
	bitans->SetMappingMode(fbxsdk::FbxLayerElement::EMappingMode::eByControlPoint);
	bitans->SetReferenceMode(fbxsdk::FbxLayerElement::EReferenceMode::eDirect);
	FbxLayerElementArrayTemplate<FbxVector4>& bitan_arr = bitans->GetDirectArray();

	std::cout << n_verts << std::endl;
	std::cout << uv_arr.GetCount() << std::endl;
	std::cout << norm_arr.GetCount() << std::endl;
	std::cout << tan_arr.GetCount() << std::endl;
	std::cout << bitan_arr.GetCount() << std::endl;

	void* vertices = malloc(vert_size * n_verts);
	char* v_scan = (char*)vertices;
	glm::vec4 temp;
	for (size_t v_i = 0; v_i < n_verts; v_i++) {
		temp.x = control_points[v_i][0];
		temp.y = control_points[v_i][1];
		temp.z = control_points[v_i][2];
		memcpy(v_scan, &temp, sizeof(glm::vec3));
		v_scan += sizeof(glm::vec3);
		temp.x = uv_arr[v_i][0];
		temp.y = uv_arr[v_i][1];
		memcpy(v_scan, &temp, sizeof(glm::vec2));
		v_scan += sizeof(glm::vec2);
		temp.x = norm_arr[v_i][0];
		temp.y = norm_arr[v_i][1];
		temp.z = norm_arr[v_i][2];
		memcpy(v_scan, &temp, sizeof(glm::vec3));
		v_scan += sizeof(glm::vec3);
		temp.x = tan_arr[v_i][0];
		temp.y = tan_arr[v_i][1];
		temp.z = tan_arr[v_i][2];
		memcpy(v_scan, &temp, sizeof(glm::vec3));
		v_scan += sizeof(glm::vec3);
		temp.x = bitan_arr[v_i][0];
		temp.y = bitan_arr[v_i][1];
		temp.z = bitan_arr[v_i][2];
		memcpy(v_scan, &temp, sizeof(glm::vec3));
		v_scan += sizeof(glm::vec3);
	}
	GH::updateWholeBuffer(result.getVertexBuffer(), vertices);
	free(vertices);
	*/

	delete[] indices;
	delete[] vertices;
	
	return result;
}

int main() {
	GHInitInfo ghii;
	ghii.dexts.push_back("VK_KHR_depth_stencil_resolve"); // to resolve depth for post-processing
	ghii.dexts.push_back("VK_KHR_create_renderpass2");    // to use above extension
	ghii.dexts.push_back("VK_KHR_multiview");             // required for renderpass2
	ghii.dexts.push_back("VK_KHR_maintenance2");          // required for renderpass2
	ghii.dexts.push_back("VK_KHR_uniform_buffer_standard_layout"); // this and below for lub to be manageable
	VkPhysicalDeviceUniformBufferStandardLayoutFeatures ubo_std_layout;
	ubo_std_layout.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES;
	ubo_std_layout.pNext = nullptr;
	ubo_std_layout.uniformBufferStandardLayout = VK_TRUE;
	ghii.pdfeats.pNext = &ubo_std_layout;

	GH gh(ghii);
	WindowInitInfo wii;
	wii.msaa = VK_SAMPLE_COUNT_4_BIT;
	WindowInfo w(wii);
	Scene s(w);

	/*
	 * Renderpasses & Pipelines
	 */
	RenderPassInfo* main_rp = createMainRenderPass(s, w);
	const size_t dns_pidx = createDNSPipeline(*main_rp, s, w);
	const size_t vp_pidx = createViewportPipeline(*main_rp, s, w);
	DNSScenePCData dns_spcd;
	main_rp->setScenePC(dns_pidx, &dns_spcd);
	main_rp->setScenePC(vp_pidx, &s.getCamera()->getVP());

	/*
	 * Lighting
	 */
	DirectionalLightInitInfo kl_ii;
	kl_ii.super_light.c = glm::vec3(1);
	kl_ii.super_directional.f = glm::normalize(glm::vec3(-1));
	DirectionalLight* key_light = s.addDirectionalLight(DirectionalLight(kl_ii), {});

	/*
	 * Textures
	 */
	TextureHandler th;
	th.addSampler("linearmag", VK_FILTER_LINEAR, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_FALSE);
	th.setDefaultSampler(th.getSampler("linearmag"));
	th.addSet("grid", TextureSet("../../resources/textures/grid", th.getSampler("linearmag")));

	/*
	 * FBX Loading
	 * Will be moved to Mesh once we have a branch for animation
	 */
	FbxManager* fbx_man = FbxManager::Create();
	FbxImporter* fbx_importer = FbxImporter::Create(fbx_man, "");

	const bool imp = fbx_importer->Initialize("resources/test.fbx", -1, fbx_man->GetIOSettings());
	if (!imp) std::cout << "import failed!" << std::endl;
	else std::cout << "import succeeded!" << std::endl;

	FbxScene* fbx_scene = FbxScene::Create(fbx_man, "");
	fbx_importer->Import(fbx_scene);	
	fbx_importer->Destroy();

	std::cout << "node count: " << fbx_scene->GetNodeCount() << std::endl;
	printNode(fbx_scene->GetNode(0));

	Mesh m = meshFromNode(fbx_scene->GetNode(findFirstMesh(fbx_scene)));
	/*
	VkDescriptorSet ds_temp;
	const TextureSet& set = th.getSet("grid");
	GH::createDS(main_rp->getRenderSet(dns_pidx).pipeline, ds_temp);
	s.addLightCatcher(&m, ds_temp, {0}, {}, {});
	GH::updateDS(ds_temp, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, set.getTexture("diffuse").getDII(), {});
	GH::updateDS(ds_temp, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, set.getTexture("normal").getDII(), {});
	GH::updateDS(ds_temp, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, set.getTexture("specular").getDII(), {});
	DNSObjectPCData m_pcd;
	main_rp->addMesh(&m, ds_temp, &m_pcd, dns_pidx);
	*/
	main_rp->addMesh(&m, VK_NULL_HANDLE, &m.getModelMatrix(), vp_pidx);

	w.addTasks(s.getDrawTasks());

	while (w.frameCallback()) {
		SDL_PumpEvents();
		float theta = (float)SDL_GetTicks() * 0.0001;
		s.getCamera()->setPos(10.f*glm::vec3(sin(theta), 1, cos(theta)));
		s.getCamera()->setForward(-s.getCamera()->getPos());
		s.getCamera()->updateView();
		s.getCamera()->updateProj();
		// m_pcd = {0, m.getModelMatrix()};
		dns_spcd = {s.getCamera()->getVP(), s.getCamera()->getPos()};
	}

	vkQueueWaitIdle(GH::getGenericQueue());

	return 0;
}

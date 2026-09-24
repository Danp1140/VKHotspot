#include "GraphicsHandler.h"
#include "Scene.h"
#include "Geometry.h"
#include <random>

void matrixTests() {
	double* m_src = new double[20];
	for (size_t n = 0; n < 20; n++) 
		m_src[n] = (double)n;
	Mat<4, 5, double> M(m_src);
	delete[] m_src;

	std::cout << M.to_string() << std::endl;
	std::cout << M.getRow(0).to_string() << std::endl;
	std::cout << M.getCol(0).to_string() << std::endl;
	std::cout << (M.getCol(0) * M.getRow(0)).to_string() << std::endl;
	std::cout << M.trans().to_string() << std::endl;
	
	m_src = new double[16];
	for (size_t n = 0; n < 16; n++) {
		if (n % 5 == 0) m_src[n] = 1;
		else m_src[n] = sin((double)n);
	}
	Mat<4, 4, double> M2(m_src);
	delete[] m_src;

	std::cout << "orig" << std::endl;
	std::cout << M2.to_string() << std::endl;
	Mat<4, 4, double> L, U, Q;
	M2.LUQ(L, U, Q);
	std::cout << "L" << std::endl;
	std::cout << L.to_string() << std::endl;
	std::cout << "U" << std::endl;
	std::cout << U.to_string() << std::endl;
	std::cout << "Q" << std::endl;
	std::cout << Q.to_string() << std::endl;
	std::cout << "check" << std::endl;
	Mat<4, 4, double> check = L * U * Q.trans();
	std::cout << check.to_string() << std::endl;

	Mat<4, 4, double> Minv = M2.invert();
	std::cout << Minv.to_string() << std::endl;
	check = M2 * Minv;
	std::cout << check.to_string() << std::endl;
	check = Minv * M2;
	std::cout << check.to_string() << std::endl;

	std::cout << "M2 det: " << std::endl;
	std::cout << M2.determinant() << std::endl;
}

void detTests() {
	Mat<2, 2, float> M1;
	for (size_t i = 0; i < 4; i++) M1.data[i] = i;
	std::cout << "det of " << std::endl;
	std::cout << M1.to_string() << std::endl;
	std::cout << "is " << M1.determinant() << std::endl;

	Mat<3, 3, float> M2;
	for (size_t i = 0; i < 9; i++) M2.data[i] = i;
	std::cout << "det of " << std::endl;
	std::cout << M2.to_string() << std::endl;
	std::cout << "is " << M2.determinant() << std::endl;
}

RenderPassInfo* createRenderPass(WindowInfo& w, Scene& s) {
	VkRenderPass r;
	VkAttachmentDescription a_d[1] {{
		0, 
		GH_SWAPCHAIN_IMAGE_FORMAT,
		VK_SAMPLE_COUNT_1_BIT,
		// VK_ATTACHMENT_LOAD_OP_LOAD,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	}};
	VkAttachmentReference a_r[1] {
		{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
	};
	GH::createRenderPass(r, 1, &a_d[0], &a_r[0], nullptr, nullptr);
	std::vector<const ImageInfo*> a_imgs = {w.getSCImages()};
	RenderPassInfo rpi(r, w.getNumSCIs(), w.getSCExtent(), {{0, 0, 0, 1}}, a_imgs, 0);
	return s.addRenderPass(rpi);
}

size_t createWireframePipeline(RenderPassInfo* rpi, const WindowInfo& w) {
	PipelineInfo p;
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "2dsimplex";
	p.vertexinputstateci = Mesh::getVISCI(VERTEX_BUFFER_TRAIT_POSITION);
	p.cullmode = VK_CULL_MODE_NONE;
	p.renderpass = rpi->getRenderPass();
	p.extent = w.getSCExtent();
	GH::createPipeline(p);
	Mesh::ungetVISCI(p.vertexinputstateci);
	return rpi->addPipeline(p, nullptr);
}

size_t createAdjPipeline(RenderPassInfo* rpi, const WindowInfo& w) {
	PipelineInfo p;
	p.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	p.shaderfilepathprefix = "2dsimplexadj";
	p.vertexinputstateci = Mesh::getVISCI(VERTEX_BUFFER_TRAIT_POSITION);
	p.cullmode = VK_CULL_MODE_NONE;
	p.renderpass = rpi->getRenderPass();
	p.extent = w.getSCExtent();
	GH::createPipeline(p);
	Mesh::ungetVISCI(p.vertexinputstateci);
	return rpi->addPipeline(p, nullptr);
}

Mesh makeDelaunayMesh2D(DelaunayGraph<2, 3>& dg) {
	Mesh m(VERTEX_BUFFER_TRAIT_POSITION, dg.getNumVertices(), dg.getNumSimplices() * 3, 0);
	glm::vec3 vert_temp[dg.getNumVertices()];
	Vec<2> vec_temp;
	for (size_t vert_i = 0; vert_i < dg.getNumVertices(); vert_i++) {
		vec_temp = *dg.getVertices()[vert_i];
		vert_temp[vert_i] = glm::vec3(vec_temp[0], vec_temp[1], vec_temp[2]);
	}
	GH::updateWholeBuffer(m.getVertexBuffer(), &vert_temp[0]);

	MeshIndex ind_temp[dg.getNumSimplices() * 3];
	size_t ind_i = 0;
	for (const Simplex<2, 3>* s : dg.getSimplices()) {
		for (Dim n = 0; n < 3; n++) {
			size_t vert_i = 0;
			while (dg.getVertices()[vert_i] != s->getMemberVertex(n)) vert_i++;
			ind_temp[ind_i] = vert_i;
			ind_i++;
		}
	}
	GH::updateWholeBuffer(m.getIndexBuffer(), &ind_temp[0]);

	return m;
}

Mesh makeDelaunayAdjMesh2D(DelaunayGraph<2, 3>& dg) {
	std::vector<Vec<2>> arrow_dirs, c1;
	for (const Simplex<2, 3>* s : dg.getSimplices()) {
		for (Dim n = 0; n < 3; n++) {
			if (s->getDirAdj()[n]) {
				c1.push_back(s->centroid());
				arrow_dirs.push_back(s->getDirAdj()[n]->centroid() - s->centroid());
				// c1.push_back(*s->getMemberVertex(n));
				// arrow_dirs.push_back(s->getDirAdj()[n]->centroid() - *s->getMemberVertex(n));
			}
		}
	}	

	Mesh m(VERTEX_BUFFER_TRAIT_POSITION, 4 * arrow_dirs.size(), 3 * 2 * arrow_dirs.size(), 0);
	glm::vec3 vert_temp[4 * arrow_dirs.size()];
	MeshIndex ind_temp[3 * 2 * arrow_dirs.size()];
	glm::vec3* vert_scan = &vert_temp[0];
	MeshIndex* ind_scan = &ind_temp[0];
	for (size_t arrow_i = 0; arrow_i < arrow_dirs.size(); arrow_i++) {
		glm::mat3 trans(1);
		trans[0][0] = arrow_dirs[arrow_i][0];
		trans[0][1] = arrow_dirs[arrow_i][1];
		glm::vec2 other_dir(-arrow_dirs[arrow_i][1], arrow_dirs[arrow_i][0]);
		other_dir = glm::normalize(other_dir);
		trans[1][0] = other_dir.x;
		trans[1][1] = other_dir.y;

		// mat so x is dir toward other, arrow_dir
		glm::vec3 offset(c1[arrow_i][0], c1[arrow_i][1], 0);
		*vert_scan++ = trans * glm::vec3(0, 0, 0) + offset;
		*vert_scan++ = trans * glm::vec3(1, 0, 0) + offset;
		*vert_scan++ = trans * glm::vec3(1, 0.03, 0) + offset;
		*vert_scan++ = trans * glm::vec3(0, 0.03, 0) + offset;

		*ind_scan++ = 4*arrow_i;
		*ind_scan++ = 4*arrow_i + 2;
		*ind_scan++ = 4*arrow_i + 1;
		*ind_scan++ = 4*arrow_i + 2;
		*ind_scan++ = 4*arrow_i;
		*ind_scan++ = 4*arrow_i + 3;
	}

	GH::updateWholeBuffer(m.getVertexBuffer(), &vert_temp[0]);
	GH::updateWholeBuffer(m.getIndexBuffer(), &ind_temp[0]);

	return m;
}

// n is square dim
// scale is length of a side
void makeGrid(size_t n, float scale, Vec<2>* dst) {
	for (size_t x = 0; x < n; x++) {
		for (size_t y = 0; y < n; y++) {
			dst[y*n + x][0] = ((float)x / (float)(n-1) - 0.5) * scale;
			dst[y*n + x][1] = ((float)y / (float)(n-1) - 0.5) * scale;
		}
	}		
}

int main() {
	detTests();

	const size_t n_pta = 4;
	Vec<2> points_to_add[n_pta];
	makeGrid(2, 0.2, &points_to_add[0]);

	DelaunayGraph<2, 3> delaunay_triangulation;
	
	for (size_t pta_i = 0; pta_i < n_pta; pta_i++)
		delaunay_triangulation.addVertex(points_to_add[pta_i]);

	Vec<2> temp;
	temp[0] = 0;
	temp[1] = -0.25;
	delaunay_triangulation.addVertex(temp);
	temp[1] = 0.4;
	delaunay_triangulation.addVertex(temp);

/*
	Simplex<2, 3>* simp;
	for (Simplex<2, 3>* sp : delaunay_triangulation.getSimplices()) {
		simp = sp;
		break;
	}
	Vec<2> test_vec;
	std::cout << "origin lies in only simplex?" << std::endl;
	std::cout << simp->vecLiesIn(test_vec) << std::endl;
	std::cout << "far-ish point is in only simplex?" << std::endl;
	test_vec[0] = 5;
	test_vec[1] = 5;
	std::cout << simp->vecLiesIn(test_vec) << std::endl;
	test_vec[0] = 0.01;
	test_vec[1] = 0.01;
	std::cout << "slightly off center is in only simplex?" << std::endl;
	std::cout << simp->vecLiesIn(test_vec) << std::endl;
*/

	
	
/*
	delaunay_triangulation.addVertex(simp);

	for (Simplex<2, 3>* sp : delaunay_triangulation.getSimplices()) {
		simp = sp;
		break;
	}
	delaunay_triangulation.addVertex(simp);
*/



	GHInitInfo ghii;
	// shouldn't be required in vulkan 1.2+, but MoltenVK sucks
	ghii.dexts.push_back("VK_KHR_depth_stencil_resolve"); 
	ghii.dexts.push_back("VK_KHR_create_renderpass2"); 
	ghii.dexts.push_back("VK_KHR_multiview"); 
	ghii.dexts.push_back("VK_KHR_maintenance2"); 
	ghii.dexts.push_back("VK_KHR_uniform_buffer_standard_layout"); 
	ghii.dps = {};
	ghii.dps.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64});
	ghii.dps.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8});
	VkPhysicalDeviceUniformBufferStandardLayoutFeatures ubo_std_layout;
	ubo_std_layout.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES;
	ubo_std_layout.pNext = nullptr;
	ubo_std_layout.uniformBufferStandardLayout = VK_TRUE;
	ghii.pdfeats.pNext = &ubo_std_layout;
	GH gh(ghii);
	
	WindowInitInfo wii;
	wii.target_display = 1;
	WindowInfo w(wii);

	Scene s(w);

	RenderPassInfo* rpi = createRenderPass(w, s);
	size_t simp_2d_pidx = createWireframePipeline(rpi, w);
	size_t simp_2d_adj_pidx = createAdjPipeline(rpi, w);

	Mesh delaunay_mesh = makeDelaunayMesh2D(delaunay_triangulation);
	rpi->addMesh(&delaunay_mesh, VK_NULL_HANDLE, nullptr, 0);
	Mesh delaunay_adj_mesh = makeDelaunayAdjMesh2D(delaunay_triangulation);
	rpi->addMesh(&delaunay_adj_mesh, VK_NULL_HANDLE, nullptr, 1);

	w.addTasks(s.getDrawTasks());

	while (w.frameCallback()) {
		SDL_PumpEvents();
	}

	vkQueueWaitIdle(GH::getGenericQueue());

	return 0;
}

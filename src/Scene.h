#include <vector>
#include <map>
#include "Projection.h"
#include "Mesh.h"

#define RENDERPASSINFO_VERBOSE_OBJECTS

typedef struct ScenePCData {
	glm::mat4 vp;
} ScenePCData;

class RenderPassInfo {
public: 
	RenderPassInfo() : renderpass(VK_NULL_HANDLE), framebuffers(nullptr), extent({0, 0}) {}
	RenderPassInfo(
		VkRenderPass r, 
		const uint32_t nsci,
		const ImageInfo* scis,
		const ImageInfo* d,
		std::vector<VkClearValue>&& c);
	~RenderPassInfo() = default;

	void destroy(); // handles actual destruction of VK objects pointed to by handles
	
	void addPipeline(const PipelineInfo& p);
	void addMesh(const Mesh* m);

	const VkRenderPass getRenderPass() const {return renderpass;}
	const VkFramebuffer* getFramebuffers() const {return framebuffers;}
	const std::vector<const Mesh*> getMeshes() const {return meshes;}
	const VkExtent2D& getExtent() const {return extent;}
	const std::vector<VkClearValue>& getClears() const {return clears;}

private:
	VkRenderPass renderpass;
	VkFramebuffer* framebuffers;
	uint32_t numscis;
	std::vector<PipelineInfo> pipelines;
	std::vector<const Mesh*> meshes;
	VkExtent2D extent;
	std::vector<VkClearValue> clears;

	void createFBs(const uint32_t nsci, const ImageInfo* scis, const ImageInfo* d);

};

class Scene {
public:
	Scene();
	~Scene();

	std::vector<cbRecTaskTemplate> getDrawTasks();

	// this should handle adding the mesh to the draw task queue
	void addRenderPass(const RenderPassInfo& r);

private:
	Camera* camera;
	std::vector<Light*> lights;
	VkDescriptorSetLayout dsl;
	VkDescriptorSet ds;
	BufferInfo lightub;
	std::vector<RenderPassInfo> renderpasses;
};

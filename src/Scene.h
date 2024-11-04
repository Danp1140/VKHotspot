#include <vector>
#include <map>
#include "Projection.h"
#include "Mesh.h"

#define RENDERPASSINFO_VERBOSE_OBJECTS

typedef struct ScenePCData {
	glm::mat4 vp;
} ScenePCData;

typedef struct RenderSet {
	PipelineInfo pipeline;
	std::vector<const Mesh*> meshes;
	const void* pcdata;
} RenderSet;

class RenderPassInfo {
	// TODO: add compute rpi
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
	
	void addPipeline(const PipelineInfo& p, const void* pcd);
	void addMesh(const Mesh* m, size_t pidx);

	std::vector<cbRecTaskTemplate> getTasks() const;

	const VkRenderPass getRenderPass() const {return renderpass;}
	const VkFramebuffer* getFramebuffers() const {return framebuffers;}
	const VkExtent2D& getExtent() const {return extent;}
	const std::vector<VkClearValue>& getClears() const {return clears;}

private:
	VkRenderPass renderpass;
	VkFramebuffer* framebuffers;
	uint32_t numscis;
	std::vector<RenderSet> rendersets;
	VkExtent2D extent;
	std::vector<VkClearValue> clears;

	void createFBs(const uint32_t nsci, const ImageInfo* scis, const ImageInfo* d);
	cbRecTaskRenderPassTemplate getRPT() const;
};

class Scene {
public:
	Scene(float a);
	~Scene();

	std::vector<cbRecTaskTemplate> getDrawTasks();

	void addRenderPass(const RenderPassInfo& r);

	const Camera* const getCamera() const {return camera;}

private:
	Camera* camera;
	std::vector<Light*> lights;
	VkDescriptorSetLayout dsl;
	VkDescriptorSet ds;
	BufferInfo lightub;
	std::vector<RenderPassInfo> renderpasses;
};

#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include <map>
#include "Projection.h"
struct RenderSet;
#include "Mesh.h"
#include "UIHandler.h"

// #define VKH_VERBOSE_DRAW_TASKS

#define SCENE_MAX_DIR_LIGHTS 8
#define SCENE_MAX_DIR_SHADOWCASTING_LIGHTS 8

typedef struct ScenePCData {
	glm::mat4 vp;
} ScenePCData;

typedef struct RenderSet {
	PipelineInfo pipeline;
	std::vector<const MeshBase*> meshes;
	std::vector<VkDescriptorSet> objdss; // associates with same-index Mesh* in meshes
	std::vector<const void*> objpcdata;
	const UIHandler* ui = nullptr; // TODO: consider specializing into Mesh, UI, Compute rendersets, etc.?
				       // structure will become clearer as UI becomes more widely used
				       // certainly should aim for efficiency and ease-of-use
	const void* pcdata;

	inline size_t findMesh(const MeshBase* m) const {
		for (size_t i = 0; i < meshes.size(); i++) if (meshes[i] == m) return i;
		FatalError("Did not find mesh in renderset").raise();
		return -1u;
	}
} RenderSet;

class RenderPassInfo {
	// TODO: add compute rpi [l]
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
	void addMesh(const MeshBase* m, VkDescriptorSet ds, const void* pc, size_t pidx);
	void setUI(const UIHandler* u, size_t pidx);

	std::vector<cbRecTaskTemplate> getTasks() const;

	const VkRenderPass getRenderPass() const {return renderpass;}
	const VkFramebuffer* getFramebuffers() const {return framebuffers;}
	const RenderSet& getRenderSet(size_t i) const {return rendersets[i];}
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

typedef struct LUBEntry {
	glm::mat4 vp;
	alignas(16) glm::vec3 p, c;
} LUBEntry;

typedef struct LUBData {
	LUBEntry e[SCENE_MAX_DIR_LIGHTS];
	alignas(16) uint32_t n;
} LUBData;

class Scene {
public:
	Scene(float a);
	~Scene();

	std::vector<cbRecTaskTemplate> getDrawTasks();

	RenderPassInfo* addRenderPass(const RenderPassInfo& r);
	// leaving updateLUB public ties into giving out a non-const DL ptr;
	// if we wanted to make updateLUB priv, we'd need some sort of check-out,
	// check-in func, but even then its still up to the user to check the ptr back in
	DirectionalLight* addDirectionalLight(DirectionalLight&& l);
	void updateLUB();

	Camera* getCamera() {return camera;}
	const DirectionalLight* getDirLights() const {return dirlights;}
	size_t getNumDirLights() const {return numdirlights;}
	const BufferInfo& getLUB() {return lightub;}
	RenderPassInfo& getRenderPass(size_t i) {return *renderpasses[i];}

private:
	Camera* camera;
	// for now just directional, point lights will require their own buffers
	// TODO: must we heap alloc the main array???
	DirectionalLight dirlights[SCENE_MAX_DIR_LIGHTS],
		* dirsclights[SCENE_MAX_DIR_SHADOWCASTING_LIGHTS];
	size_t numdirlights, numdirsclights;
	VkDescriptorSetLayout dsl;
	VkDescriptorSet ds;
	BufferInfo lightub;
	std::vector<RenderPassInfo*> renderpasses;
};

#endif

#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include <map>
#include "Projection.h"
struct RenderSet;
#include "Mesh.h"
#include "UIHandler.h"

// #define VKH_VERBOSE_DRAW_TASKS

// for various reasons, these cannot get above 256
#define SCENE_MAX_DIR_LIGHTS uint8_t(8) 
#define SCENE_MAX_DIR_SHADOWCASTING_LIGHTS uint8_t(8)
// this one can tho
#define SCENE_MAX_SHADOWCATCHERS uint32_t(64)

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
		const ImageInfo* ms,
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

	// TODO: see if you can make this interface more intuitive
	void createFBs(const uint32_t nsci, const ImageInfo* scis, const ImageInfo* r, const ImageInfo* d);
	cbRecTaskRenderPassTemplate getRPT() const;
};

// TODO: consider using std430 for all below
typedef struct LUBLightEntry {
	glm::mat4 vp;
	alignas(16) glm::vec3 p, c;
} LUBEntry;

// TODO: as numcatchers grows, figure out a system that wastes less space on padding
typedef struct LUBCatcherEntry {
	uint32_t nscdls, ndls;
	// could pack four idxs into these
	// issue is this: in std140, all arrays are in 16-byte segments
	// solution: to avoid extension dependence, we'll store in vec4s
	/*
	alignas(16) uint32_t scdlidxs[SCENE_MAX_DIR_SHADOWCASTING_LIGHTS],
		dlidxs[SCENE_MAX_DIR_LIGHTS];
*/
	alignas(16) glm::uvec4 scdlidxs[SCENE_MAX_DIR_SHADOWCASTING_LIGHTS / 4],
		dlidxs[SCENE_MAX_DIR_LIGHTS / 4];
	uint32_t padding[256 / 4 - (4 + SCENE_MAX_DIR_SHADOWCASTING_LIGHTS + SCENE_MAX_DIR_LIGHTS)];
} LUBCatcherEntry;

typedef struct LUBData {
	LUBLightEntry dle[SCENE_MAX_DIR_LIGHTS];
	alignas(16) LUBLightEntry scdle[SCENE_MAX_DIR_SHADOWCASTING_LIGHTS];
	alignas(16) LUBCatcherEntry ce[SCENE_MAX_SHADOWCATCHERS];
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

	void hookupShadowCaster(const MeshBase* m, std::vector<uint32_t>&& scdlidxs);
	// gotta update three descriptor sets: LUB, SM array, and CUB
	// diff btwn below two functions is that hookup increments numcatchers and 
	// writes LUB DS. both with write SM array and CUB DS
	void hookupLightCatcher(
		const MeshBase* m, 
		const VkDescriptorSet& ds, 
		const std::vector<uint32_t>& dlidxs, 
		const std::vector<uint32_t>& scdlidxs);
	void updateLightCatcher(
		const MeshBase* m, 
		const VkDescriptorSet& ds, 
		const std::vector<uint32_t>& dlidxs, 
		const std::vector<uint32_t>& scdlidxs,
		uint32_t cidx);

	// TODO: func to add light to catcher
	// really just needs to update that catcher's cub; lub should already be updated

	Camera* getCamera() {return camera;}
	const DirectionalLight* getDirLights() const {return dirlights;}
	const DirectionalLight* getDirSCLights() const {return dirsclights;}
	size_t getNumDirLights() const {return numdirlights;}
	size_t getNumDirSCLights() const {return numdirsclights;}
	const BufferInfo& getLUB() {return lightub;}
	RenderPassInfo& getRenderPass(size_t i) {return *renderpasses[i];}

private:
	Camera* camera;
	DirectionalLight dirlights[SCENE_MAX_DIR_LIGHTS],
		dirsclights[SCENE_MAX_DIR_SHADOWCASTING_LIGHTS];
	size_t numdirlights, numdirsclights, numcatchers;
	VkDescriptorSetLayout dsl;
	VkDescriptorSet ds;
	BufferInfo lightub;
	std::vector<RenderPassInfo*> renderpasses;
};

#endif

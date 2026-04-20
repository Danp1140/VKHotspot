#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include <map>
#include <set>
#include "Projection.h"
struct RenderSet;
#include "Mesh.h"
#include "UIHandler.h"

// #define VKH_VERBOSE_DRAW_TASKS

// TODO: when/if LUB size becomes an issue, we can change these to uint8_t and bitmask
// in shader
typedef uint32_t LightIndex;
// for various reasons, these cannot get above 256
#define SCENE_MAX_DIR_LIGHTS ((LightIndex)8) 
#define SCENE_MAX_SPOT_LIGHTS ((LightIndex)8)
#define SCENE_MAX_POINT_LIGHTS ((LightIndex)8)
#define SCENE_MAX_SC_LIGHTS ((LightIndex)8)
// this one can tho
#define SCENE_MAX_SHADOW_CATCHERS ((uint32_t)64)

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
	VkViewport viewport; // only used if pipeline has dynamic viewport state
	VkRect2D scissor; // same as above

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
	RenderPassInfo(VkRenderPass r, const uint32_t nsci, VkExtent2D ext, std::vector<VkClearValue>&& c, std::vector<const ImageInfo*> att, uint8_t sci_att_idx);
	~RenderPassInfo() = default;

	void destroy(); // handles actual destruction of VK objects pointed to by handles

	// returns the index of the newly-added pipeline
	size_t addPipeline(const PipelineInfo& p, const void* pcd);
	size_t addPipeline(const PipelineInfo& p, const void* pcd, VkViewport vp, VkRect2D sc);
	void setScenePC(size_t pidx, const void* pcd) {rendersets[pidx].pcdata = pcd;}
	void addMesh(const MeshBase* m, VkDescriptorSet ds, const void* pc, size_t pidx);
	void setUI(const UIHandler* u, size_t pidx); // TODO: prob get rid of this

	std::vector<cbRecTaskTemplate> getTasks() const;

	const VkRenderPass getRenderPass() const {return renderpass;}
	const VkFramebuffer* getFramebuffers() const {return framebuffers;}
	const RenderSet& getRenderSet(size_t i) const {return rendersets[i];}
	const VkExtent2D& getExtent() const {return extent;}
	const std::vector<VkClearValue>& getClears() const {return clears;}
	cbRecTaskRenderPassTemplate getRPT() const;

private:
	VkRenderPass renderpass;
	VkFramebuffer* framebuffers;
	uint32_t numscis;
	std::vector<RenderSet> rendersets;
	VkExtent2D extent;
	std::vector<VkClearValue> clears;

	// TODO: see if you can make this interface more intuitive
	void createFBs(const uint32_t nsci, const ImageInfo* scis, const ImageInfo* r, const ImageInfo* d);
	void createFBs(const uint32_t nsci, uint32_t sciidx, const std::vector<const ImageInfo*> imgs);
};

typedef struct SMEntry {
	glm::mat4 vp;
	glm::vec4 uv_ext_off;
} SMData;

// light counts are an array here and in LUB to ease alignment woes
typedef struct CatcherEntry {
	LightIndex light_counts[4]; // dir, spot, pt, and padding 
	// TODO: don't let any catcher have ALL lights
	// there can be much more lights in the scene than one catcher is allowed to catch
	alignas(16) LightIndex dir_light_idxs[SCENE_MAX_DIR_LIGHTS],
		spot_light_idxs[SCENE_MAX_SPOT_LIGHTS],
		point_light_idxs[SCENE_MAX_POINT_LIGHTS];
} CatcherEntry;

/*
 * Below is unlikely to ever be instantiated on the CPU
 * It is simply a framework for the buffer copies using sizeof and offsetof
 */
#define SCENE_UBO_MIN_OFFSET 256 // presumed
typedef struct LUBData {
	LightIndex light_counts[4]; // dir, spot, pt, and padding 
	// contains c, f, and p (as required for each)
	// really only needs vec4 but UBOs must be padded here to 16 even in array
	alignas(16) glm::vec4 vectors[2*SCENE_MAX_DIR_LIGHTS + 3*SCENE_MAX_SPOT_LIGHTS + 2*SCENE_MAX_POINT_LIGHTS];
	alignas(16) uint32_t sm_idxs[SCENE_MAX_DIR_LIGHTS + SCENE_MAX_SPOT_LIGHTS + SCENE_MAX_POINT_LIGHTS]; // maps all light idxs to sm_entries, same ordering convention as vectors
	alignas(16) SMEntry sm_entries[SCENE_MAX_SC_LIGHTS];

	// TODO this maybe doesn't have to live in the same struct, allowing for dynamic padding when we make the buffer
	alignas(SCENE_UBO_MIN_OFFSET) CatcherEntry catcher_entries[SCENE_MAX_SHADOW_CATCHERS];
} LUBData;

class Scene {
public:
	Scene(float a);
	Scene(const WindowInfo& w) : Scene((float)w.getSCExtent().width / (float)w.getSCExtent().height) {}
	~Scene();

	std::vector<cbRecTaskTemplate> getDrawTasks();

	// returns index to just-added renderpass;
	RenderPassInfo* addRenderPass(const RenderPassInfo& r);
	// leaving updateLUB public ties into giving out a non-const DL ptr;
	// if we wanted to make updateLUB priv, we'd need some sort of check-out,
	// check-in func, but even then its still up to the user to check the ptr back in
	DirectionalLight* addDirectionalLight(const DirectionalLight& l, const std::vector<VkExtent2D>& sm_exts);
	std::vector<size_t> addSMPipeline(const Light& l, const PipelineInfo& p, RenderPassInfo& rpi, const void* pcd);

	void addShadowCaster(const MeshBase* m, std::vector<uint32_t>&& scdlidxs);
	// gotta update three descriptor sets: LUB, SM array, and CUB
	// diff btwn below two functions is that hookup increments numcatchers and 
	// writes LUB DS. both with write SM array and CUB DS
	uint32_t addLightCatcher(
		const MeshBase* m, 
		const VkDescriptorSet& ds, 
		const std::vector<uint32_t>& d_l_idxs, 
		const std::vector<uint32_t>& s_l_idxs, 
		const std::vector<uint32_t>& p_l_idxs);
	void updateLightCatcher(
		const MeshBase* m, 
		const VkDescriptorSet& ds, 
		const std::vector<uint32_t>& d_l_idxs, 
		const std::vector<uint32_t>& s_l_idxs, 
		const std::vector<uint32_t>& p_l_idxs,
		uint32_t cidx);
	void updateSMD(Light& l, size_t smd_idx);

	// TODO: func to add light to catcher
	// really just needs to update that catcher's cub; lub should already be updated

	Camera* getCamera() {return camera;}
	const DirectionalLight* getDirLights() const {return dir_lights;}
	size_t getNumDirLights() const {return n_dir_lights;}
	const BufferInfo& getLUB() {return lightub;}
	RenderPassInfo& getRenderPass(size_t i) {return *renderpasses[i];}
	const ImageInfo& getShadowAtlas() {return shadow_atlas;}

private:
	Camera* camera;

	DirectionalLight dir_lights[SCENE_MAX_DIR_LIGHTS];
	SpotLight spot_lights[SCENE_MAX_SPOT_LIGHTS];
	PointLight point_lights[SCENE_MAX_POINT_LIGHTS];
	LightIndex n_dir_lights, n_spot_lights, n_point_lights, n_sc_lights;
	uint32_t n_catchers;
	BufferInfo lightub;
	ImageInfo shadow_atlas;
	VkExtent2D sa_offset;
	void* cub_redund; // used to accomodate UBO offset alignment, re-writing data that is already there 
	size_t cub_alignment;

	VkDescriptorSetLayout dsl;
	VkDescriptorSet ds;
	std::vector<RenderPassInfo*> renderpasses;

	/*
	 * VERY basic implementation
	 * assumes you never free anything
	 * assumes texes are 1024 square 
	 * really just inserts them in a tile
	 */
	VkExtent2D getSAZone(VkExtent2D ext);
	/*
	 * In practice should probably never be called.
	 * Stop-gap until we get a more detailed update system working
	 */
	void updateWholeLUB();
};

#endif

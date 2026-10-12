#include "GraphicsHandler.h"

typedef struct PPRenderSet {
	PipelineInfo pipeline;
	VkDescriptorSet ds; 
	const void* pcdata;
	VkRenderPass rp;
	VkFramebuffer* fb;
} PPRenderSet;

typedef struct temp_pc_dat {
	glm::mat4 vp_c, vp_l, vp_c_inv;
	glm::vec3 c_p; float t; 
} temp_pc_dat;

typedef enum PPStepInitFlagsBits {
	PPS_INIT_FLAGS_NONE = 0x00,
	PPS_INIT_FLAGS_HAS_SRC_IMG_BIT = 0x01,
	PPS_INIT_FLAGS_HAS_DEPTH_IMG_BIT = 0x02,
	PPS_INIT_FLAGS_HAS_DEPTH_RESOLVE_BIT = 0x04
} PPStepInitFlagsBits;
typedef uint8_t PPStepInitFlags;

typedef struct PPStepInitInfo {
	const WindowInfo* w = nullptr;
	VkRenderPass rp = VK_NULL_HANDLE; // if null, a new one is made
	VkFramebuffer* fbs = nullptr; // cannot be nullptr if rp is not null
	PipelineInfo pipeline = {};
	PPStepInitFlags flags = PPS_INIT_FLAGS_NONE;
	ImageInfo src = {};
	const ImageInfo* dsts = nullptr;
	uint8_t n_dsts = 0;
	void* pcd = nullptr;
	bool last = false;
} PPStepInitInfo;

class PPStep {
public:
	PPStep(const WindowInfo* w, const char* shader_fpp);
	PPStep(const PPStepInitInfo ii);
	~PPStep();

	PPRenderSet getRS() const {return {pipeline, ds, pcd, rp, fb};}
	cbRecTaskRenderPassTemplate getRTRPT() const {return cbRecTaskRenderPassTemplate(
			rp, 
			fb,
			n_dsts, 
			dsts[0].extent,
			1, &clear);}

	VkRenderPass getRP() const {return rp;}
	const VkFramebuffer* getFB() const {return fb;}
	VkDescriptorSet getDS() const {return ds;}
	const PipelineInfo& getPipeline() const {return pipeline;}
	const ImageInfo& getSrc() const {return src;}
	const ImageInfo& getDepthRes() const {return depth_res;}

	void updatePC(temp_pc_dat t) {temp = t;}

	static void recordCopy(uint8_t scii, VkCommandBuffer& c, const ImageInfo* src, const ImageInfo& dst);
	static void recordDepthResolve(uint8_t scii, VkCommandBuffer& c, const ImageInfo& ms_db, const ImageInfo& db);
	static void recordDraw(uint8_t scii, VkCommandBuffer& c, const PPRenderSet& rs);

private:
	const WindowInfo* window;
	PipelineInfo pipeline;
	VkRenderPass rp;
	VkClearValue clear;
	VkFramebuffer* fb;
	VkDescriptorSet ds;
	temp_pc_dat temp;
	void* pcd;
	ImageInfo src, depth_res;
	const ImageInfo* dsts;
	uint8_t n_dsts;

	void createRenderPass(bool last);
	void createPipeline(); 
};

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

class PPStep {
public:
	PPStep(const WindowInfo* w, const char* shader_fpp);
	~PPStep();

	PPRenderSet getRS() const {return {pipeline, ds, &temp, rp, fb};}
	cbRecTaskRenderPassTemplate getRTRPT() const {return cbRecTaskRenderPassTemplate(
			rp, 
			fb,
			window->getNumSCIs(), 
			window->getSCExtent(),
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
	ImageInfo src, depth_res;

	void createRenderPass();
	void createPipeline(); 
};

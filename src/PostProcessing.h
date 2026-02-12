#include "GraphicsHandler.h"

typedef struct PPRenderSet {
	PipelineInfo pipeline;
	VkDescriptorSet ds; 
	const void* pcdata;
	VkRenderPass rp, 
	VkFramebuffer* fb
} PPRenderSet;

typedef struct temp_pc_dat {
	glm::mat4 vp_c, vp_l;
} temp_pc_dat;

class PPStep {
public:
	PPStep(const WindowInfo* w, const char* shader_fpp);
	~PPStep();

	VkDescriptorSet getDS() const {return ds;}
	PPRenderSet getRS() const {return {pipeline, ds, &temp};}
	cbRecTaskRenderPassTemplate getRTRPT() const {return cbRecTaskRenderPassTemplate(
			rp, 
			fb,
			window->getNumSCIs(), 
			window->getSCExtent(),
			1, &clear);}

	VkRenderPass getRP() const {return rp;}
	const VkFramebuffer* getFB() const {return fb;}
	VkDescriptorSet getDrawDS() const {return draw_ds;}
	const PipelineInfo& getDrawPipeline() const {return draw_pipeline;}

	static void recordDraw(uint8_t scii, VkCommandBuffer& c, const PPRenderSet& rs);

private:
	const WindowInfo* window;
	PipelineInfo pipeline;
	VkRenderPass rp;
	VkClearValue clear;
	VkFramebuffer* fb;
	VkDescriptorSet ds;
	temp_pc_dat temp;
	ImageInfo dst;

	void createRenderPass();
	void createDrawPipeline(); 
};

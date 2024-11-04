#ifndef GRAPHICS_HANDLER_H
#define GRAPHICS_HANDLER_H

#include <vulkan/vulkan.h>
#include <ext.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <functional>

#include "Errors.h"

#define GH_SWAPCHAIN_IMAGE_FORMAT VK_FORMAT_B8G8R8A8_SRGB
// TODO: constider making this a D16_UNORM
// this intersects with system-dependent format selection with preferential formats, as not all systems
// may be capable of a D16_UNORM, while most if not all will be able to use a D32_SFLOAT
#define GH_DEPTH_BUFFER_IMAGE_FORMAT VK_FORMAT_D32_SFLOAT
#define GH_MAX_FRAMES_IN_FLIGHT 6
// TODO: get these out of here asap
#define WORKING_DIRECTORY "/Users/danp/Desktop/C Coding/WaveBox/"

#define NUM_SHADER_STAGES_SUPPORTED 5
const VkShaderStageFlagBits supportedshaderstages[NUM_SHADER_STAGES_SUPPORTED] = {
	VK_SHADER_STAGE_COMPUTE_BIT,
	VK_SHADER_STAGE_VERTEX_BIT,
	VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
	VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
	VK_SHADER_STAGE_FRAGMENT_BIT
};
const char* const shaderstagestrs[NUM_SHADER_STAGES_SUPPORTED] = {
	"comp",
	"vert",
	"tesc",
	"tese",
	"frag"
};

const VkCommandBufferBeginInfo interimcbbegininfo {
	VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	nullptr,
	VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	nullptr
};

/*
 * Vulkan Type Wrapper Structs
 */

typedef struct BufferInfo {
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkBufferUsageFlags usage = 0x0;
	VkMemoryPropertyFlags memprops = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	VkDeviceSize size;
} BufferInfo;

typedef struct ImageInfo {
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkExtent2D extent = {0, 0};
	VkFormat format = VK_FORMAT_UNDEFINED;
	VkImageUsageFlags usage = 0u;
	VkImageLayout layout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	VkSampler sampler = VK_NULL_HANDLE;
	VkMemoryPropertyFlags memprops = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	VkImageTiling tiling; // set by ci depending on img props

	constexpr VkImageSubresource getDefaultSubresource() const {
		return {
			format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
			0, 0
		};
	}
	constexpr VkImageSubresourceRange getDefaultSubresourceRange() const {
		return {
			format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
			0, 1,
			0, 1
		};
	}
	constexpr VkDescriptorImageInfo getDII() const {
		return {sampler, view, layout};
	}
	constexpr size_t getPixelSize() const {
		switch (format) { 
			case VK_FORMAT_R8_UNORM:
				return 1;
			case VK_FORMAT_R8G8B8A8_SRGB:
				return 4;
			default:
				FatalError("Unknown format for getPixelSize()").raise();
				return -1u;
		}
	}
} ImageInfo;

typedef struct PipelineInfo {
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkShaderStageFlags stages = 0u;
	const char* shaderfilepathprefix = nullptr;
	VkDescriptorSetLayoutCreateInfo descsetlayoutci = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO
	};
	VkPushConstantRange pushconstantrange = {};
	VkPipelineVertexInputStateCreateInfo vertexinputstateci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
	};
	bool depthtest = false;
	VkSpecializationInfo specinfo = {};
	VkPrimitiveTopology topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkExtent2D extent = {0, 0}; 
	VkCullModeFlags cullmode = VK_CULL_MODE_BACK_BIT;
	VkRenderPass renderpass;
} PipelineInfo;

typedef std::function<void (VkCommandBuffer&)> cbRecFunc;

typedef enum cbRecTaskType {
		CB_REC_TASK_TYPE_UNINITIALIZED,
		CB_REC_TASK_TYPE_COMMAND_BUFFER,
		CB_REC_TASK_TYPE_RENDERPASS,
		CB_REC_TASK_TYPE_DEPENDENCY,
} cbRecTaskType;

typedef struct cbRecTask {
	cbRecTask () : type(CB_REC_TASK_TYPE_UNINITIALIZED), data() {}

	explicit cbRecTask (cbRecFunc f) : type(CB_REC_TASK_TYPE_COMMAND_BUFFER) {
		new(&data.func) cbRecFunc(f);
	}

	explicit cbRecTask (VkRenderPassBeginInfo r) : type(CB_REC_TASK_TYPE_RENDERPASS) {
		data.rpbi = r;
	}

	explicit cbRecTask (VkDependencyInfoKHR d) : type(CB_REC_TASK_TYPE_DEPENDENCY) {
		data.di = d;
	}

	cbRecTask (const cbRecTask& c) : type(c.type) {
		if (type == CB_REC_TASK_TYPE_COMMAND_BUFFER) new(&data.func) cbRecFunc(c.data.func);
		else if (type == CB_REC_TASK_TYPE_RENDERPASS) data.rpbi = c.data.rpbi;
		else data.di = c.data.di;
	}

	void operator= (const cbRecTask& c) {
		type = c.type;
		if (type == CB_REC_TASK_TYPE_COMMAND_BUFFER) new(&data.func) cbRecFunc(c.data.func);
		else if (type == CB_REC_TASK_TYPE_RENDERPASS) data.rpbi = c.data.rpbi;
		else data.di = c.data.di;
	}

	~cbRecTask () {
		if (type == CB_REC_TASK_TYPE_COMMAND_BUFFER) data.func.~function();
		type.~cbRecTaskType();
		data.~cbRecTaskData();
	}

	cbRecTaskType type;

	union cbRecTaskData {
		cbRecTaskData () {}

		~cbRecTaskData () {} // union destructors are impossible to write...too bad!

		cbRecFunc func;
		VkRenderPassBeginInfo rpbi;
		VkDependencyInfoKHR di;
	} data;
} cbRecTask;

typedef struct cbCollectInfo {
	explicit cbCollectInfo (const VkCommandBuffer& c) : 
		type(CB_COLLECT_INFO_TYPE_COMMAND_BUFFER), 
		data {.cmdbuf = c} {}

	explicit cbCollectInfo (const VkRenderPassBeginInfo& r) : 
		type(CB_COLLECT_INFO_TYPE_RENDERPASS), 
		data {.rpbi = r} {}

	explicit cbCollectInfo (const VkDependencyInfoKHR& d) : 
		type(CB_COLLECT_INFO_TYPE_DEPENDENCY), 
		data {.di = d} {}

	enum cbCollectInfoType {
		CB_COLLECT_INFO_TYPE_COMMAND_BUFFER,
		CB_COLLECT_INFO_TYPE_RENDERPASS,
		CB_COLLECT_INFO_TYPE_DEPENDENCY
	} type;

	union cbCollectInfoData {
		VkCommandBuffer cmdbuf;
		VkRenderPassBeginInfo rpbi;
		VkDependencyInfoKHR di;
	} data;
} cbCollectInfo;

typedef std::function<void (uint8_t, VkCommandBuffer&)> cbRecFuncTemplate;

typedef struct cbRecTaskRenderPassTemplate {
	VkRenderPass rp;
	const VkFramebuffer* fbs;
	VkExtent2D ext;
	uint32_t nclears;
	const VkClearValue* clears;

	cbRecTaskRenderPassTemplate() = delete;
	cbRecTaskRenderPassTemplate(
		const VkRenderPass r,
		const VkFramebuffer* const f,
		const VkExtent2D e,
		uint32_t nc,
		const VkClearValue* const c) :
		rp(r),
		fbs(f),
		ext(e),
		nclears(nc),
		clears(c) {}
	~cbRecTaskRenderPassTemplate() {}
} cbRecTaskRenderPassTemplate;

typedef struct cbRecTaskTemplate {
	cbRecTaskTemplate() = default;
	cbRecTaskTemplate(cbRecFuncTemplate f) {
		type = CB_REC_TASK_TYPE_COMMAND_BUFFER;
		// data.ft = f;
		// still no clue what this line does
		new(&data.ft) cbRecFuncTemplate(f);
	}
	cbRecTaskTemplate(cbRecTaskRenderPassTemplate r) {
		type = CB_REC_TASK_TYPE_RENDERPASS;
		data.rpi = r;
	}
	cbRecTaskTemplate(const cbRecTaskTemplate& rhs) : type(rhs.type) {
		if (rhs.type == CB_REC_TASK_TYPE_COMMAND_BUFFER) {
			// data.ft = rhs.data.ft;
			new(&data.ft) cbRecFuncTemplate(rhs.data.ft);
		}
		else if (rhs.type == CB_REC_TASK_TYPE_RENDERPASS) {
			data.rpi = rhs.data.rpi;
		}
	}
	~cbRecTaskTemplate() {
		if (type == CB_REC_TASK_TYPE_COMMAND_BUFFER) {
			if (data.ft) data.ft.~function();
		}
	}

	void operator= (const cbRecTaskTemplate& rhs)  {
		type = rhs.type;
		if (rhs.type == CB_REC_TASK_TYPE_COMMAND_BUFFER) {
			new(&data.ft) cbRecFuncTemplate(rhs.data.ft);
		}
		else if (rhs.type == CB_REC_TASK_TYPE_RENDERPASS) {
			data.rpi = rhs.data.rpi;
		}
	}


	cbRecTaskType type;

	union cbRecTaskTemplateData {
		cbRecTaskTemplateData () {}
		~cbRecTaskTemplateData () {}

		cbRecFuncTemplate ft;
		cbRecTaskRenderPassTemplate rpi;
	} data;
} cbRecTaskTemplate;

class GH;

class WindowInfo {
public:
	/*
	 * Potential Params:
	 * - Window size
	 * - Window position
	 * - Window title
	 * - Window decoration
	 * - Fullscreen/Borderless Windowed
	 * - Resizeable?
	 * - Depth buffer?
	 * - Resolution (can this vary relative to window size?)
	 * - Monitor
	 */
	WindowInfo();
	// explicitly delete these until we can safely implement them
	WindowInfo(const WindowInfo& lvalue) = delete;
	WindowInfo(WindowInfo&& rvalue) = delete;
	~WindowInfo();

	void frameCallback();
	void addTask(const cbRecTaskTemplate& t);
	void addTasks(std::vector<cbRecTaskTemplate>&& t);

	const VkSwapchainKHR& getSwapchain() const {return swapchain;}
	const VkSemaphore& getImgAcquireSema() const {return imgacquiresema;}
	const ImageInfo* const getSCImages() const {return scimages;}
	const ImageInfo* const getDepthBuffer() const {return &depthbuffer;}
	const VkExtent2D& getSCExtent() const {return scimages[0].extent;}
	uint32_t getNumSCIs() const {return numscis;}

private:
	SDL_Window* sdlwindow;
	uint32_t sdlwindowid;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	ImageInfo* scimages, depthbuffer;
	uint32_t numscis, sciindex, fifindex;
	VkSemaphore imgacquiresema, subfinishsemas[GH_MAX_FRAMES_IN_FLIGHT];
	VkFence subfinishfences[GH_MAX_FRAMES_IN_FLIGHT];
	VkCommandBuffer primarycbs[GH_MAX_FRAMES_IN_FLIGHT];
	std::vector<cbRecTask>* rectaskvec;
	std::queue<cbRecTask> rectasks; // TODO: rename to rectaskqueue
	std::queue<cbCollectInfo> collectinfos;
	std::vector<VkCommandBuffer> secondarycbset[GH_MAX_FRAMES_IN_FLIGHT];

	// below members are temp to make ops done every frame faster
	// these are used directly after they're set, and should not be read elsewhere
	static const VkPipelineStageFlags defaultsubmitwaitstage;
	static const VkCommandBufferBeginInfo primarycbbegininfo; 
	static VkCommandBufferAllocateInfo cballocinfo;
	VkSubmitInfo submitinfo;
	VkPresentInfoKHR presentinfo;

	void createSyncObjects();
	void destroySyncObjects();

	void createPrimaryCBs();
	void destroyPrimaryCBs();

	// when we multithread the recording process we'll need to add mutexes to processRecordingTasks
	// and have a VkCommandPool for each thread (in each window, unless we wanted GH handling
	// threads for some reason)
	static void processRecordingTasks(
		uint8_t fifindex,
		uint8_t threadindex,
		std::queue<cbRecTask>& rectasks,
		std::queue<cbCollectInfo>& collectinfos,
		std::vector<VkCommandBuffer>& secondarycbset);
	void collectPrimaryCB();
	void submitAndPresent();
};

class GH {
public:
	GH();
	~GH();

	static void createRenderPass(
		VkRenderPass& rp,
		uint8_t numattachments,
		VkAttachmentDescription* attachmentdescs,
		VkAttachmentReference* colorattachmentrefs,
		VkAttachmentReference* depthattachmentref);
	static void destroyRenderPass(VkRenderPass& rp);

	static void createPipeline(PipelineInfo& pi);
	static void destroyPipeline(PipelineInfo& pi);

	static void createDS(const PipelineInfo& p, VkDescriptorSet& ds);
	static void updateDS(
		const VkDescriptorSet& ds, 
		uint32_t i,
		VkDescriptorType t,
		VkDescriptorImageInfo ii,
		VkDescriptorBufferInfo bi);
	static void updateWholeDS(
		const VkDescriptorSet& ds, 
		std::vector<VkDescriptorType>&& t,
		std::vector<VkDescriptorImageInfo>&& ii,
		std::vector<VkDescriptorBufferInfo>&& bi);

	static void createBuffer(BufferInfo& b);
	static void destroyBuffer(BufferInfo& b);
	static void updateWholeBuffer(const BufferInfo& b, void* src);

	/*
	 * Creates image & image view and allocates memory. Non-default values for all other members should be set
	 * in i before calling createImage.
	 */
	static void createImage(ImageInfo& i);
	static void destroyImage(ImageInfo& i);
	// TODO: make work with DEVICE_LOCAL memory/images
	static void updateImage(ImageInfo& i, void* src);

	// unsure if an entire other secondary cb is inefficient here
	/*
	static void recordPushConsts(
		VkShaderStageFlags stages,
		uint32_t offset,
		uint32_t size,
		const void* pc,
		cbRecData d, 
		VkCommandBuffer& c);
		*/

	static const VkInstance& getInstance() {return instance;}
	static const VkDevice& getLD() {return logicaldevice;}
	static const VkPhysicalDevice& getPD() {return physicaldevice;}
	static const VkQueue& getGenericQueue() {return genericqueue;}
	static const uint8_t getQueueFamilyIndex() {return queuefamilyindex;}
	static const VkCommandPool& getCommandPool() {return commandpool;}
	static const VkClearValue* const getPresentationClearsPtr() {return &primaryclears[0];}
	static const VkSampler& getNearestSampler() {return nearestsampler;}

	static void setShaderDirectory(const char* d) {shaderdir = d;}

private:
	static VkInstance instance;
	VkDebugUtilsMessengerEXT debugmessenger;
	static VkDevice logicaldevice;
	static VkPhysicalDevice physicaldevice;
	static VkQueue genericqueue;
	static uint8_t queuefamilyindex;
	static VkCommandPool commandpool;
	static VkCommandBuffer interimcb; // unsure if this is an efficient model, but will use until proven not to be
	static VkFence interimfence; // created initCommandPools w/ interimcb 
	static VkDescriptorPool descriptorpool;
	// TODO: re-check which of these are necessary after getting a bare-bones draw loop finished
	static const VkClearValue primaryclears[2];
	static VkSampler nearestsampler;
	static BufferInfo scratchbuffer;
	static const char* shaderdir;

	/*
	 * Below are several graphics initialization functions. Most have self-explanatory names and are relatively
	 * simple and so are not worth exhaustively commenting on. Generally, those with names create_____ or 
	 * destroy_____ could reasonably be called multiple times. However, those with names init______ or 
	 * terminate_______ should only be called once per GH, likely in the constructor and destructor respectively.
	 * Some functions have sparse comments for things they do that are not self-explanatory.
	 */

	// init/terminateVulkanInstance also handle the primary surface & window, as they will always exist for a GH
	// TODO: Remove hard-coding and enhance initVulkanInstance's ability to dynamically enable needed extensions.
	void initVulkanInstance();
	void terminateVulkanInstance();

	static VkResult createDebugMessenger(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* createinfo,
		const VkAllocationCallbacks* allocator,
		VkDebugUtilsMessengerEXT* debugutilsmessenger);
	static void destroyDebugMessenger(
		VkInstance instance,
		VkDebugUtilsMessengerEXT debugutilsmessenger,
		const VkAllocationCallbacks* allocator);
	void initDebug();
	void terminateDebug();

	// TODO: As in initVulkanInstance, remove hard-coding and dynamically find best extensions, queue families, 
	// and hardware to use
	void initDevicesAndQueues();
	void terminateDevicesAndQueues();

	void initCommandPools();
	void terminateCommandPools();

	void initSamplers();
	void terminateSamplers();

	static void initDescriptorPoolsAndSetLayouts();
	static void terminateDescriptorPoolsAndSetLayouts();

	static void transitionImageLayout(ImageInfo& i, VkImageLayout newlayout);

	static void createShader(
		VkShaderStageFlags stages,
		const char** filepaths,
		VkShaderModule*& modules,
		VkPipelineShaderStageCreateInfo** createinfos,
		VkSpecializationInfo* specializationinfos);
	static void destroyShader(VkShaderModule shader);

	static void allocateBufferMemory(BufferInfo& b);
	static void allocateImageMemory(ImageInfo& i);
	static void allocateDeviceMemory(
		const VkMemoryPropertyFlags mp, 
		const VkMemoryRequirements mr, 
		VkDeviceMemory& m);
	static void freeDeviceMemory(VkDeviceMemory& m);

	static VKAPI_ATTR VkBool32 VKAPI_CALL validationCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT severity,
			VkDebugUtilsMessageTypeFlagsEXT type,
			const VkDebugUtilsMessengerCallbackDataEXT* callbackdata,
			void* userdata);
};
#endif

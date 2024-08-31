#include <vulkan.h>
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
#define GH_DEPTH_BUFFER_IMAGE_FORMAT VK_FORMAT_D32_SFLOAT
#define GH_MAX_FRAMES_IN_FLIGHT 6
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

/*
 * Vulkan Type Wrapper Structs
 */

typedef struct ImageInfo {
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkExtent2D extent = {0, 0};
	VkFormat format = VK_FORMAT_UNDEFINED;
	VkImageUsageFlags usage = 0u;

	VkImageSubresourceRange getDefaultSubresourceRange() const {
		return {
			format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
			0, 1,
			0, 1
		};
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
	VkRenderPass renderpass = VK_NULL_HANDLE; // if not set, will default to primaryrenderpass
} PipelineInfo;

typedef std::function<void (VkCommandBuffer&)> cbRecFunc;

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

	enum cbRecTaskType {
		CB_REC_TASK_TYPE_UNINITIALIZED,
		CB_REC_TASK_TYPE_COMMAND_BUFFER,
		CB_REC_TASK_TYPE_RENDERPASS,
		CB_REC_TASK_TYPE_DEPENDENCY,
	} type;

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
	~WindowInfo();

	void frameCallback();

	const VkSwapchainKHR& getSwapchain() const {return swapchain;}
	const VkSemaphore& getImgAcquireSema() const {return imgacquiresema;}

private:
	SDL_Window* sdlwindow;
	uint32_t sdlwindowid;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	ImageInfo* scimages, depthbuffer;
	uint32_t numscis, sciindex, fifindex;
	VkSemaphore imgacquiresema, subfinishsemas[GH_MAX_FRAMES_IN_FLIGHT];
	VkFence subfinishfences[GH_MAX_FRAMES_IN_FLIGHT];
	// TODO: better framebuffer management system (intersects with GH renderpass system)
	VkFramebuffer* presentationfbs;
	VkCommandBuffer primarycbs[GH_MAX_FRAMES_IN_FLIGHT];
	std::queue<cbRecTask> rectasks;
	std::queue<cbCollectInfo> collectinfos;
	std::vector<VkCommandBuffer> secondarycbset;

	// below members are temp to make ops done every frame faster
	// these are used directly after they're set, and should not be read elsewhere
	static const VkPipelineStageFlags defaultsubmitwaitstage;
	static const VkCommandBufferBeginInfo primarycbbegininfo; 
	static const VkCommandBufferAllocateInfo cballocinfo;
	VkSubmitInfo submitinfo;
	VkPresentInfoKHR presentinfo;

	void createSyncObjects();
	void destroySyncObjects();

	void createPresentationFBs();
	void destroyPresentationFBs();

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

	void createPipeline(PipelineInfo& pi);
	void destroyPipeline(PipelineInfo& pi);
	/*
	 * Creates image & image view and allocates memory. Non-default values for all other members should be set
	 * in i before calling createImage.
	 */
	static void createImage(ImageInfo& i);
	static void destroyImage(ImageInfo& i);

	static const VkInstance& getInstance() {return instance;}
	static const VkDevice& getLD() {return logicaldevice;}
	static const VkPhysicalDevice& getPD() {return physicaldevice;}
	static const VkQueue& getGenericQueue() {return genericqueue;}
	static const uint8_t getQueueFamilyIndex() {return queuefamilyindex;}
	// TODO: renderpass should not be a static member
	// should have a system for managing an arbitrary number (or none)
	static const VkRenderPass& getPresentationRenderPass() {return primaryrenderpass;}
	static const VkCommandPool& getCommandPool() {return commandpool;}
	static const VkClearValue* const getPresentationClearsPtr() {return &primaryclears[0];}

private:
	static VkInstance instance;
	VkDebugUtilsMessengerEXT debugmessenger;
	static VkDevice logicaldevice;
	static VkPhysicalDevice physicaldevice;
	static VkQueue genericqueue;
	static uint8_t queuefamilyindex;
	static VkRenderPass primaryrenderpass;
	static VkCommandPool commandpool;
	VkDescriptorPool descriptorpool;
	// TODO: re-check which of these are necessary after getting a bare-bones draw loop finished
	static const VkClearValue primaryclears[2];

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

	void initRenderpasses();
	void terminateRenderpasses();

	void initCommandPools();
	void terminateCommandPools();

	void initDescriptorPoolsAndSetLayouts();
	void terminateDescriptorPoolsAndSetLayouts();
	
	void createShader(
		VkShaderStageFlags stages,
		const char** filepaths,
		VkShaderModule** modules,
		VkPipelineShaderStageCreateInfo** createinfos,
		VkSpecializationInfo* specializationinfos);
	void destroyShader(VkShaderModule shader);

	// TODO: BufferInfo class
	static void allocateDeviceMemory(
		const VkBuffer& buffer,
		const VkImage& image,
		VkDeviceMemory& memory,
		VkMemoryPropertyFlags memprops);
	static void freeDeviceMemory(VkDeviceMemory& memory);

	static VKAPI_ATTR VkBool32 VKAPI_CALL validationCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT severity,
			VkDebugUtilsMessageTypeFlagsEXT type,
			const VkDebugUtilsMessengerCallbackDataEXT* callbackdata,
			void* userdata);
};

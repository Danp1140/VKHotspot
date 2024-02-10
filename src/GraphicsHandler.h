#include <vulkan/vulkan.h>
#include <glm/ext.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>

#define GH_SWAPCHAIN_IMAGE_FORMAT VK_FORMAT_B8G8R8A8_SRGB
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



typedef struct ImageInfo {
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkExtent2D extent = {0, 0};
	VkFormat format = VK_FORMAT_UNDEFINED;
	VkImageUsageFlags usage = 0u;
} ImageInfo;

typedef struct PipelineInfo {
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	VkShaderStageFlags stages = 0u;
	const char* shaderfilepathprefix = nullptr;
	VkDescriptorSetLayoutCreateInfo descsetlayoutci = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	VkPushConstantRange pushconstantrange = {};
	VkPipelineVertexInputStateCreateInfo vertexinputstateci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
	bool depthtest = false;
	VkSpecializationInfo specinfo = {};
	VkPrimitiveTopology topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkExtent2D extent = {0, 0}; // in createPipeline, this is defaulted to the swapchain extent if left zeroed
	VkCullModeFlags cullmode = VK_CULL_MODE_BACK_BIT;
	VkRenderPass renderpass = VK_NULL_HANDLE; // if not set, will default to primaryrenderpass
} PipelineInfo;


class GH {
public:
	GH();
	~GH();

	void loop();

	bool endLoop() {return glfwWindowShouldClose(primarywindow);}

	void createPipeline(PipelineInfo& pi);
	void destroyPipeline(PipelineInfo& pi);
	/*
	 * Creates image & image view and allocates memory. Non-default values for all other members should be set
	 * in i before calling createImage.
	 */
	void createImage(ImageInfo& i);
	void destroyImage(ImageInfo& i);

private:
	GLFWwindow* primarywindow;
	VkInstance instance;
	VkSurfaceKHR primarysurface;
	VkDebugUtilsMessengerEXT debugmessenger;
	VkDevice logicaldevice;
	VkPhysicalDevice physicaldevice;
	VkQueue genericqueue;
	VkSwapchainKHR swapchain;
	VkExtent2D swapchainextent; // TODO: eliminate this, get directly from scis
	uint8_t fifindex, queuefamilyindex;
	uint32_t sciindex, numsci;
	ImageInfo* swapchainimages, depthbuffer;
	VkRenderPass primaryrenderpass;
	VkFramebuffer* framebuffers;
	VkCommandPool commandpool;
	VkCommandBuffer* primarycommandbuffers;
	VkDescriptorPool descriptorpool;
	// TODO: re-check which of these are necessary after getting a bare-bones draw loop finished
	VkSemaphore* imageacquiredsemaphores,* submitfinishedsemaphores;
	VkFence* submitfinishedfences;
	const VkClearValue primaryclears[2] = {{0.1, 0.1, 0.1, 1.}, 0.};

	/*
	 * Below are several graphics initialization functions. Most have self-explanatory names and are relatively
	 * simple and so are not worth exhaustively commenting on. Generally, those with names create_____ or 
	 * destroy_____ could reasonably be called multiple times. However, those with names init______ or 
	 * terminate_______ should only be called once per GH, likely in the constructor and destructor respectively.
	 * Some functions have sparse comments for things they do that are not self-explanatory.
	 */
	// TODO: Ensure createWindow checks if glfw is initialized before calling glfwInit()
	void createWindow(GLFWwindow*& w);
	void destroyWindow(GLFWwindow*& w);
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
	void initSwapchain();
	void terminateSwapchain();
	void initRenderpasses();
	void terminateRenderpasses();
	void initDepthBuffer();
	void terminateDepthBuffer();
	void initFramebuffers();
	void terminateFramebuffers();
	// Due to low total draw numbers, for now we will only need one thread and so one of each pool
	void initCommandPools();
	void terminateCommandPools();
	void initCommandBuffers();
	void terminateCommandBuffers();
	void initDescriptorPoolsAndSetLayouts();
	void terminateDescriptorPoolsAndSetLayouts();
	void initSyncObjects();
	void terminateSyncObjects();

	void createShader(
		VkShaderStageFlags stages,
		const char** filepaths,
		VkShaderModule** modules,
		VkPipelineShaderStageCreateInfo** createinfos,
		VkSpecializationInfo* specializationinfos);
	void destroyShader(VkShaderModule shader);

	void allocateDeviceMemory(
		const VkBuffer& buffer,
		const VkImage& image,
		VkDeviceMemory& memory,
		VkMemoryPropertyFlags memprops);
	void freeDeviceMemory(VkDeviceMemory& memory);

	void recordPrimaryCommandBuffer();
	void submitAndPresent();

	static VKAPI_ATTR VkBool32 VKAPI_CALL validationCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT severity,
			VkDebugUtilsMessageTypeFlagsEXT type,
			const VkDebugUtilsMessengerCallbackDataEXT* callbackdata,
			void* userdata);
};

#include "GraphicsHandler.h"

GH::GH() {
	initVulkanInstance();
	initDebug();
	initDevicesAndQueues();
	initSwapchain();
	initRenderpasses();
	initDepthBuffer();
	initFramebuffers();
	initCommandPools();
	initCommandBuffers();
	initDescriptorPoolsAndSetLayouts();
	initSyncObjects();
}

GH::~GH() {
	vkQueueWaitIdle(genericqueue);
	terminateSyncObjects();
	terminateDescriptorPoolsAndSetLayouts();
	terminateCommandBuffers();
	terminateCommandPools();
	terminateFramebuffers();
	terminateDepthBuffer();
	terminateRenderpasses();
	terminateSwapchain();
	terminateDevicesAndQueues();
	terminateDebug();
	terminateVulkanInstance();
}

void GH::loop() {
	vkAcquireNextImageKHR(logicaldevice,
			      swapchain,
			      UINT64_MAX,
			      imageacquiredsemaphores[sciindex],
			      VK_NULL_HANDLE,
			      &sciindex);
	glfwPollEvents();
	recordPrimaryCommandBuffer();
	submitAndPresent();
	glfwSwapBuffers(primarywindow); // unsure of proper buffer swap timing
	fifindex++;
	if (fifindex == GH_MAX_FRAMES_IN_FLIGHT) fifindex = 0;
}

void GH::createWindow(GLFWwindow*& w) {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	w = glfwCreateWindow(mode->width, mode->height, "WaveBox", nullptr, nullptr);
	glfwMakeContextCurrent(w);
}

void GH::destroyWindow(GLFWwindow*& w) {
	glfwDestroyWindow(w);
	glfwTerminate();
}

void GH::initVulkanInstance() {
	createWindow(primarywindow);
	VkApplicationInfo appinfo {
		VK_STRUCTURE_TYPE_APPLICATION_INFO,
		nullptr,
		"WaveBox",
		VK_MAKE_VERSION(1, 0, 0),
		"Jet",
		VK_MAKE_VERSION(1, 0, 0),
		VK_MAKE_API_VERSION(0, 1, 0, 0)
	};

	const char* layers[1] {"VK_LAYER_KHRONOS_validation"};
	const char* extensions[5] {
		"VK_MVK_macos_surface",
		"VK_KHR_surface",
		"VK_EXT_metal_surface",
		"VK_KHR_get_physical_device_properties2",
		"VK_EXT_debug_utils"
	};
	VkInstanceCreateInfo instancecreateinfo {
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		nullptr,
		0,
		&appinfo,
		1, &layers[0],
		5, &extensions[0]
	};
	vkCreateInstance(&instancecreateinfo, nullptr, &instance);

	glfwCreateWindowSurface(instance, primarywindow, nullptr, &primarysurface);

}

void GH::terminateVulkanInstance() {
	vkDestroySurfaceKHR(instance, primarysurface, nullptr);
	vkDestroyInstance(instance, nullptr);
	destroyWindow(primarywindow);
}

VkResult GH::createDebugMessenger(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* createinfo,
	const VkAllocationCallbacks* allocator,
	VkDebugUtilsMessengerEXT* debugutilsmessenger) {
	auto funcptr = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	return funcptr(instance, createinfo, allocator, debugutilsmessenger);
}

void GH::destroyDebugMessenger(
	VkInstance instance,
	VkDebugUtilsMessengerEXT debugutilsmessenger,
	const VkAllocationCallbacks* allocator) {
auto funcptr = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	funcptr(instance, debugutilsmessenger, allocator);
}

void GH::initDebug() {
	VkDebugUtilsMessengerCreateInfoEXT debugmessengercreateinfo {
		VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		nullptr,
		0,
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		validationCallback,
		nullptr
	};
	createDebugMessenger(instance, &debugmessengercreateinfo, nullptr, &debugmessenger);
}

void GH::terminateDebug() {
	destroyDebugMessenger(instance, debugmessenger, nullptr);
}

void GH::initDevicesAndQueues() {
	uint32_t numphysicaldevices = -1u,
			 numqueuefamilies;
	vkEnumeratePhysicalDevices(instance, &numphysicaldevices, &physicaldevice);
	vkGetPhysicalDeviceQueueFamilyProperties(physicaldevice, &numqueuefamilies, nullptr);
	VkQueueFamilyProperties queuefamilyprops[numqueuefamilies];
	vkGetPhysicalDeviceQueueFamilyProperties(physicaldevice, &numqueuefamilies, &queuefamilyprops[0]);
	VkBool32 surfacesupport;
	queuefamilyindex = 0;
	vkGetPhysicalDeviceSurfaceSupportKHR(physicaldevice, queuefamilyindex, primarysurface, &surfacesupport);
	const float priorities[1] = {1.f};
	VkDeviceQueueCreateInfo queuecreateinfo {
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		nullptr,
		0,
		queuefamilyindex,
		1,
		&priorities[0]
	};
	const char* deviceextensions[2] {
		"VK_KHR_swapchain",
		"VK_KHR_portability_subset"
	};
	VkPhysicalDeviceFeatures physicaldevicefeatures {};
	VkDeviceCreateInfo devicecreateinfo {
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		nullptr,
		0,
		1, &queuecreateinfo,
		0, nullptr,
		2, &deviceextensions[0],
		&physicaldevicefeatures
	};
	vkCreateDevice(physicaldevice, &devicecreateinfo, nullptr, &logicaldevice);

	vkGetDeviceQueue(logicaldevice, queuefamilyindex, 0, &genericqueue);
}

void GH::terminateDevicesAndQueues() {
	vkDestroyDevice(logicaldevice, nullptr);
}

void GH::initSwapchain() {
	VkSurfaceCapabilitiesKHR surfacecaps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicaldevice, primarysurface, &surfacecaps);
	numsci = surfacecaps.maxImageCount;
	VkSwapchainCreateInfoKHR swapchaincreateinfo {
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		nullptr,
		0,
		primarysurface,
		surfacecaps.maxImageCount,
		GH_SWAPCHAIN_IMAGE_FORMAT,
		VK_COLORSPACE_SRGB_NONLINEAR_KHR,
		surfacecaps.currentExtent,
		1,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0, nullptr,
		surfacecaps.currentTransform,
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_PRESENT_MODE_FIFO_KHR,
		VK_TRUE,
		VK_NULL_HANDLE
	};
	vkCreateSwapchainKHR(logicaldevice, &swapchaincreateinfo, nullptr, &swapchain);
	vkGetSwapchainImagesKHR(logicaldevice, swapchain, &numsci, nullptr);
	swapchainimages = new ImageInfo[numsci];
	VkImage scitemp[numsci];
	vkGetSwapchainImagesKHR(logicaldevice, swapchain, &numsci, &scitemp[0]);
	for (sciindex = 0; sciindex < numsci; sciindex++) swapchainimages[sciindex].image = scitemp[sciindex];
	for (sciindex = 0; sciindex < numsci; sciindex++) {
		swapchainimages[sciindex].extent = surfacecaps.currentExtent;
		swapchainimages[sciindex].memory = VK_NULL_HANDLE;
		swapchainimages[sciindex].format = GH_SWAPCHAIN_IMAGE_FORMAT;
		VkImageViewCreateInfo imageviewcreateinfo {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			nullptr,
			0,
			swapchainimages[sciindex].image,
			VK_IMAGE_VIEW_TYPE_2D,
			GH_SWAPCHAIN_IMAGE_FORMAT,
			{VK_COMPONENT_SWIZZLE_IDENTITY,
			 VK_COMPONENT_SWIZZLE_IDENTITY,
			 VK_COMPONENT_SWIZZLE_IDENTITY,
			 VK_COMPONENT_SWIZZLE_IDENTITY},
			{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
		};
		vkCreateImageView(logicaldevice, &imageviewcreateinfo, nullptr, &swapchainimages[sciindex].view);
	}
	sciindex = 0u;
}

void GH::terminateSwapchain() {
	vkDestroySwapchainKHR(logicaldevice, swapchain, nullptr);
	for (uint8_t scii = 0; scii < numsci; scii++) {
		vkDestroyImageView(logicaldevice, swapchainimages[scii].view, nullptr);
	}
	delete[] swapchainimages;
}

void GH::initRenderpasses() {
	// If you ever have to make more than one renderpass, move all this nonsense to a createRenderpass function
	// w/ a struct input info
	VkAttachmentDescription primaryattachmentdescriptions[2] {{
		0,                                      // color attachment
		GH_SWAPCHAIN_IMAGE_FORMAT,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	}, {
		0,                                      // depth attachment
		VK_FORMAT_D32_SFLOAT,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	}};
	VkAttachmentReference primaryattachmentreferences[2] {
		{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
		{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}
	};
	VkSubpassDescription primarysubpassdescription {
		0,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		0, nullptr,
		1, &primaryattachmentreferences[0], nullptr, &primaryattachmentreferences[1],
		0, nullptr
	};
	VkSubpassDependency subpassdependency {
		VK_SUBPASS_EXTERNAL, 0,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		0
	};
	VkRenderPassCreateInfo primaryrenderpasscreateinfo {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		nullptr,
		0,
		2, &primaryattachmentdescriptions[0],
		1, &primarysubpassdescription,
		1, &subpassdependency
	};
	vkCreateRenderPass(logicaldevice, &primaryrenderpasscreateinfo, nullptr, &primaryrenderpass);
}

void GH::terminateRenderpasses() {
	vkDestroyRenderPass(logicaldevice, primaryrenderpass, nullptr);
}

void GH::initDepthBuffer() {
	depthbuffer.extent = swapchainimages[0].extent;
	depthbuffer.format = VK_FORMAT_D32_SFLOAT;
	depthbuffer.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	createImage(depthbuffer);
}

void GH::terminateDepthBuffer() {
	destroyImage(depthbuffer);
}

void GH::initFramebuffers() {
	framebuffers = new VkFramebuffer[numsci];
	VkImageView attachments[2];
	attachments[1] = depthbuffer.view;
	VkFramebufferCreateInfo framebufferci {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		nullptr,
		0,
		primaryrenderpass,
		2, &attachments[0],
		swapchainimages[0].extent.width, swapchainimages[0].extent.height, 1
	};
	for (uint8_t scii = 0; scii < numsci; scii++) {
		attachments[0] = swapchainimages[scii].view;
		vkCreateFramebuffer(logicaldevice, &framebufferci, nullptr, &framebuffers[scii]);
	}
}

void GH::terminateFramebuffers() {
	for (uint8_t scii = 0; scii < numsci; scii++) {
		vkDestroyFramebuffer(logicaldevice, framebuffers[scii], nullptr);
	}
	delete[] framebuffers;
}
void GH::initCommandPools() {
	VkCommandPoolCreateInfo commandpoolci {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		nullptr,
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		queuefamilyindex
	};
	vkCreateCommandPool(logicaldevice, &commandpoolci, nullptr, &commandpool);
}

void GH::terminateCommandPools() {
	vkDestroyCommandPool(logicaldevice, commandpool, nullptr);
}

void GH::initCommandBuffers() {
	fifindex = 0u;
	primarycommandbuffers = new VkCommandBuffer[GH_MAX_FRAMES_IN_FLIGHT];
	VkCommandBufferAllocateInfo commandbufferai {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		nullptr,
		commandpool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		GH_MAX_FRAMES_IN_FLIGHT,
	};
	vkAllocateCommandBuffers(logicaldevice, &commandbufferai, &primarycommandbuffers[0]);
}

void GH::terminateCommandBuffers() {
	vkFreeCommandBuffers(logicaldevice, commandpool, GH_MAX_FRAMES_IN_FLIGHT, &primarycommandbuffers[0]);
	delete[] primarycommandbuffers;
}

void GH::initDescriptorPoolsAndSetLayouts() {
	VkDescriptorPoolSize poolsizes[0];
	VkDescriptorPoolCreateInfo descriptorpoolci {
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		nullptr,
		0,
		0,
		0, nullptr
	};
	// uncomment this and line in terminateDescriptorPoolsAndSetLayouts when we actually have a DS to alloc
	// vkCreateDescriptorPool(logicaldevice, &descriptorpoolci, nullptr, &descriptorpool);
}

void GH::terminateDescriptorPoolsAndSetLayouts() {
	// vkDestroyDescriptorPool(logicaldevice, descriptorpool, nullptr);
}

void GH::initSyncObjects() {
	imageacquiredsemaphores = new VkSemaphore[numsci];
	VkSemaphoreCreateInfo imageacquiredsemaphorecreateinfo {
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		nullptr,
		0
	};
	for (uint8_t scii = 0; scii < numsci; scii++) {
		vkCreateSemaphore(logicaldevice, &imageacquiredsemaphorecreateinfo, nullptr, &imageacquiredsemaphores[scii]);
	}
	submitfinishedfences = new VkFence[GH_MAX_FRAMES_IN_FLIGHT];
	VkFenceCreateInfo submitfinishedfencecreateinfo {
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		nullptr,
		VK_FENCE_CREATE_SIGNALED_BIT
	};
	submitfinishedsemaphores = new VkSemaphore[GH_MAX_FRAMES_IN_FLIGHT];
	VkSemaphoreCreateInfo submitfinishedsemaphorecreateinfo {
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		nullptr,
		0
	};
	for (uint8_t fifi = 0; fifi < GH_MAX_FRAMES_IN_FLIGHT; fifi++) {
		vkCreateFence(logicaldevice, &submitfinishedfencecreateinfo, nullptr, &submitfinishedfences[fifi]);
		vkCreateSemaphore(logicaldevice, &submitfinishedsemaphorecreateinfo, nullptr, &submitfinishedsemaphores[fifi]);
	}
}

void GH::terminateSyncObjects() {
	for (uint8_t fifi = 0; fifi < GH_MAX_FRAMES_IN_FLIGHT; fifi++) {
		if (fifi < numsci) vkDestroySemaphore(logicaldevice, imageacquiredsemaphores[fifi], nullptr);
		vkDestroyFence(logicaldevice, submitfinishedfences[fifi], nullptr);
		vkDestroySemaphore(logicaldevice, submitfinishedsemaphores[fifi], nullptr);
	}
	delete[] submitfinishedsemaphores;
	delete[] submitfinishedfences;
	delete[] imageacquiredsemaphores;
}

void GH::createPipeline (PipelineInfo& pi) {
	if (pi.stages & VK_SHADER_STAGE_COMPUTE_BIT) {
		vkCreateDescriptorSetLayout(logicaldevice,
						&pi.descsetlayoutci,
						nullptr,
						&pi.dsl);
		VkPipelineLayoutCreateInfo pipelinelayoutci {
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			nullptr,
			0,
			1, &pi.dsl,
			pi.pushconstantrange.size == 0 ? 0u : 1u, &pi.pushconstantrange
		};
		vkCreatePipelineLayout(logicaldevice,
					&pipelinelayoutci,
					nullptr,
					&pi.layout);
		VkShaderModule* shadermodule = new VkShaderModule;
		VkPipelineShaderStageCreateInfo* shaderstagecreateinfo = new VkPipelineShaderStageCreateInfo;
		std::string tempstr = std::string(WORKING_DIRECTORY "resources/shaders/SPIRV/")
			.append(pi.shaderfilepathprefix)
			.append("comp.spv");
		const char* filepath = tempstr.c_str();
		createShader(VK_SHADER_STAGE_COMPUTE_BIT,
						 &filepath,
						 &shadermodule,
						 &shaderstagecreateinfo,
						 nullptr);
		VkComputePipelineCreateInfo pipelinecreateinfo {
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			nullptr,
			0,
			*shaderstagecreateinfo,
			pi.layout,
			VK_NULL_HANDLE,
			1
		};
		vkCreateComputePipelines(logicaldevice,
						 VK_NULL_HANDLE,
						 1,
						 &pipelinecreateinfo,
						 nullptr,
						 &pi.pipeline);
		delete shaderstagecreateinfo;
		destroyShader(*shadermodule);
		delete shadermodule;
		return;
	}

	uint32_t numshaderstages = 0;
	char* shaderfilepaths[NUM_SHADER_STAGES_SUPPORTED];
	std::string temp;
	for (uint8_t i = 0; i < NUM_SHADER_STAGES_SUPPORTED; i++) {
		if (supportedshaderstages[i] & pi.stages) {
			temp = std::string(WORKING_DIRECTORY "resources/shaders/SPIRV/")
				.append(pi.shaderfilepathprefix)
				.append(shaderstagestrs[i])
				.append(".spv");
			shaderfilepaths[numshaderstages] = new char[temp.length() + 1]; // adding 1 for null terminator
			temp.copy(shaderfilepaths[numshaderstages], temp.length());
			shaderfilepaths[numshaderstages][temp.length()] = '\0';
			numshaderstages++;
		}
	}
	VkShaderModule* shadermodules = new VkShaderModule[numshaderstages];
	VkPipelineShaderStageCreateInfo* shaderstagecreateinfos = new VkPipelineShaderStageCreateInfo[numshaderstages];
	createShader(pi.stages,
			 const_cast<const char**>(&shaderfilepaths[0]),
			 &shadermodules,
			 &shaderstagecreateinfos,
			 nullptr);
	
	vkCreateDescriptorSetLayout(logicaldevice,
					&pi.descsetlayoutci,
					nullptr,
					&pi.dsl);
	VkPipelineLayoutCreateInfo pipelinelayoutcreateinfo {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1, &pi.dsl,
		pi.pushconstantrange.size == 0 ? 0u : 1u, &pi.pushconstantrange
	};
	vkCreatePipelineLayout(logicaldevice,
				&pipelinelayoutcreateinfo,
				nullptr,
				&pi.layout);

	if ((pi.stages & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) && (pi.topo != VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)) {
		std::cout << "createPipeline warning!!! Are you sure you want your tessellated pipeline (" 
			<< pi.shaderfilepathprefix 
			<< ") to use a primitive topology other than patch list?" << std::endl;
	}
	VkPipelineInputAssemblyStateCreateInfo inputassemblystatecreateinfo {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		nullptr,
		0,
		pi.topo,
		VK_FALSE
	};
	VkPipelineTessellationStateCreateInfo tessstatecreateinfo {
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		nullptr,
		0,
		3
	};
	if (pi.extent.width == 0) pi.extent = swapchainimages[0].extent;
	VkViewport viewporttemp {
		0.0f, 0.0f,
		float(pi.extent.width), float(pi.extent.height),
		0.0f, 1.0f
	};
	VkRect2D scissortemp {
		{0, 0},
		pi.extent
	};
	VkPipelineViewportStateCreateInfo viewportstatecreateinfo {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		nullptr,
		0,
		1,
		&viewporttemp,
		1,
		&scissortemp
	};
	VkPipelineRasterizationStateCreateInfo rasterizationstatecreateinfo {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_FALSE,
		VK_POLYGON_MODE_FILL,
		pi.cullmode,
		VK_FRONT_FACE_COUNTER_CLOCKWISE,
		VK_FALSE,
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};
	VkPipelineMultisampleStateCreateInfo multisamplestatecreateinfo {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FALSE,
		1.0f,
		nullptr,
		VK_FALSE,
		VK_FALSE
	};
	VkPipelineDepthStencilStateCreateInfo depthstencilstatecreateinfo;
	if (pi.depthtest) {
		depthstencilstatecreateinfo = {
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_TRUE,
			VK_TRUE,
			VK_COMPARE_OP_LESS,
			VK_FALSE,
			VK_FALSE,
			{},
			{},
			0.0f,
			1.0f
		};
	} else {
		depthstencilstatecreateinfo = {
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_FALSE,
			VK_FALSE,
			VK_COMPARE_OP_NEVER,
			VK_FALSE,
			VK_FALSE,
			{},
			{},
			0.0f,
			1.0f
		};
	}
	VkPipelineColorBlendAttachmentState colorblendattachmentstate {
		VK_TRUE,
		VK_BLEND_FACTOR_SRC_ALPHA,
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		VK_BLEND_OP_ADD,
		VK_BLEND_FACTOR_SRC_ALPHA,
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		VK_BLEND_OP_SUBTRACT,
		VK_COLOR_COMPONENT_R_BIT 
			| VK_COLOR_COMPONENT_G_BIT 
			| VK_COLOR_COMPONENT_B_BIT 
			| VK_COLOR_COMPONENT_A_BIT
	};
	VkPipelineColorBlendStateCreateInfo colorblendstatecreateinfo {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_LOGIC_OP_AND,
		1, &colorblendattachmentstate,
		{0.0f, 0.0f, 0.0f, 0.0f}
	};
	if (pi.renderpass == VK_NULL_HANDLE) pi.renderpass = primaryrenderpass;
	VkGraphicsPipelineCreateInfo pipelinecreateinfo = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		nullptr,
		0,
		numshaderstages,
		shaderstagecreateinfos,
		&pi.vertexinputstateci,
		&inputassemblystatecreateinfo,
		pi.stages & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ? &tessstatecreateinfo : nullptr,
		&viewportstatecreateinfo,
		&rasterizationstatecreateinfo,
		&multisamplestatecreateinfo,
		&depthstencilstatecreateinfo,
		&colorblendstatecreateinfo,
		nullptr,
		pi.layout,
		pi.renderpass,
		0,
		VK_NULL_HANDLE,
		-1
	};
	vkCreateGraphicsPipelines(logicaldevice,
				  VK_NULL_HANDLE,
				  1,
				  &pipelinecreateinfo,
				  nullptr,
				  &pi.pipeline);

	delete[] shaderstagecreateinfos;
	for (unsigned char x = 0; x < numshaderstages; x++) {
		delete shaderfilepaths[x];
		destroyShader(shadermodules[x]);
	}
	delete[] shadermodules;
}

void GH::destroyPipeline(PipelineInfo& pi) {
	vkDestroyPipeline(logicaldevice, pi.pipeline, nullptr);
	vkDestroyPipelineLayout(logicaldevice, pi.layout, nullptr);
	vkDestroyDescriptorSetLayout(logicaldevice, pi.dsl, nullptr);
}

void GH::createShader(
		VkShaderStageFlags stages,
		const char** filepaths,
		VkShaderModule** modules,
		VkPipelineShaderStageCreateInfo** createinfos,
		VkSpecializationInfo* specializationinfos) {
	std::ifstream filestream;
	size_t shadersrcsize;
	char* shadersrc;
	unsigned char stagecounter = 0;
	VkShaderModuleCreateInfo modcreateinfo;
	for (unsigned char x = 0; x < NUM_SHADER_STAGES_SUPPORTED; x++) {
		if (stages & supportedshaderstages[x]) {
			filestream = std::ifstream(filepaths[stagecounter], std::ios::ate | std::ios::binary);
			shadersrcsize = filestream.tellg();
			shadersrc = new char[shadersrcsize];
			filestream.seekg(0);
			filestream.read(&shadersrc[0], shadersrcsize);
			filestream.close();
			modcreateinfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			modcreateinfo.pNext = nullptr;
			modcreateinfo.flags = 0;
			modcreateinfo.codeSize = shadersrcsize;
			modcreateinfo.pCode = reinterpret_cast<const uint32_t*>(&shadersrc[0]);
			vkCreateShaderModule(logicaldevice,
								 &modcreateinfo,
								 nullptr,
								 &(*modules)[stagecounter]);
			(*createinfos)[stagecounter].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			(*createinfos)[stagecounter].pNext = nullptr;
			(*createinfos)[stagecounter].flags = 0;
			(*createinfos)[stagecounter].stage = supportedshaderstages[x];
			(*createinfos)[stagecounter].module = (*modules)[stagecounter];
			(*createinfos)[stagecounter].pName = "main";
			(*createinfos)[stagecounter].pSpecializationInfo = specializationinfos
															   ? &specializationinfos[stagecounter]
															   : nullptr;
			delete[] shadersrc;
			stagecounter++;
		}
	}
}

void GH::destroyShader(VkShaderModule shader) {
	vkDestroyShaderModule(logicaldevice, shader, nullptr);
}

void GH::allocateDeviceMemory(
	const VkBuffer& buffer,
	const VkImage& image,
	VkDeviceMemory& memory,
	VkMemoryPropertyFlags memprops) {
	if (buffer == VK_NULL_HANDLE && image == VK_NULL_HANDLE) {
		throw std::runtime_error("allocateDeviceMemory error: both buffer and image are VK_NULL_HANDLE");
	}
	if (buffer != VK_NULL_HANDLE && image != VK_NULL_HANDLE) {
		throw std::runtime_error("allocateDeviceMemory error: both buffer and image are defined");
	}
	VkMemoryRequirements memreqs;
	if (buffer != VK_NULL_HANDLE) vkGetBufferMemoryRequirements(logicaldevice, buffer, &memreqs);
	else vkGetImageMemoryRequirements(logicaldevice, image, &memreqs);
	VkPhysicalDeviceMemoryProperties physicaldevicememprops;
	vkGetPhysicalDeviceMemoryProperties(physicaldevice, &physicaldevicememprops);
	uint32_t finalmemindex = -1u;
	for (uint32_t memindex = 0; memindex < physicaldevicememprops.memoryTypeCount; memindex++) {
		if (memreqs.memoryTypeBits & (1 << memindex)
		    && physicaldevicememprops.memoryTypes[memindex].propertyFlags & memprops) {
			finalmemindex = memindex;
			break;
		}
	}
	if (finalmemindex == -1u) {
		throw std::runtime_error("Couldn't find appropriate memory to allocate");
	}
	VkMemoryAllocateInfo memoryai {
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			nullptr,
			memreqs.size,
			finalmemindex
	};
	vkAllocateMemory(logicaldevice, &memoryai, nullptr, &memory);
}

void GH::freeDeviceMemory(VkDeviceMemory& memory) {
	vkFreeMemory(logicaldevice, memory, nullptr);
}

void GH::createImage(ImageInfo& i) {
	VkImageCreateInfo imageci {
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		nullptr,
		0,
		VK_IMAGE_TYPE_2D,
		i.format,
		{i.extent.width, i.extent.height, 1},
		1,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_OPTIMAL,
		i.usage,
		VK_SHARING_MODE_EXCLUSIVE,
		0, nullptr,
		VK_IMAGE_LAYOUT_UNDEFINED
	};
	vkCreateImage(logicaldevice, &imageci, nullptr, &i.image);

	allocateDeviceMemory(VK_NULL_HANDLE, i.image, i.memory, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vkBindImageMemory(logicaldevice, i.image, i.memory, 0);

	VkImageAspectFlags aspect = (i.format == VK_FORMAT_D32_SFLOAT) ? 
		VK_IMAGE_ASPECT_DEPTH_BIT : 
		VK_IMAGE_ASPECT_COLOR_BIT;
	VkImageViewCreateInfo imageviewci {
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		nullptr,
		0,
		i.image,
		VK_IMAGE_VIEW_TYPE_2D,
		i.format,
		{VK_COMPONENT_SWIZZLE_IDENTITY,
		 VK_COMPONENT_SWIZZLE_IDENTITY,
		 VK_COMPONENT_SWIZZLE_IDENTITY,
		 VK_COMPONENT_SWIZZLE_IDENTITY},
		{aspect, 0, 1, 0, 1}
	};
	vkCreateImageView(logicaldevice, &imageviewci, nullptr, &i.view);
}

void GH::destroyImage(ImageInfo& i) {
	vkDestroyImageView(logicaldevice, i.view, nullptr);
	freeDeviceMemory(i.memory);
	vkDestroyImage(logicaldevice, i.image, nullptr);
}

void GH::recordPrimaryCommandBuffer() {
	VkCommandBufferBeginInfo primarycommandbufferbi {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		nullptr,
		0,
		nullptr
	};
	vkWaitForFences(logicaldevice, 1, &submitfinishedfences[fifindex], VK_FALSE, UINT64_MAX);
	vkBeginCommandBuffer(primarycommandbuffers[fifindex], &primarycommandbufferbi);
	VkRenderPassBeginInfo rpbi {
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		nullptr,
		primaryrenderpass,
		framebuffers[sciindex],
		{{0, 0}, swapchainimages[sciindex].extent},
		2,
		&primaryclears[0]
	};
	vkCmdBeginRenderPass(primarycommandbuffers[fifindex], &rpbi, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdEndRenderPass(primarycommandbuffers[fifindex]);
	vkEndCommandBuffer(primarycommandbuffers[fifindex]);
}

void GH::submitAndPresent() {
	VkPipelineStageFlags waitstage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	vkResetFences(logicaldevice, 1, &submitfinishedfences[fifindex]);
	VkSubmitInfo submitinfo {
		VK_STRUCTURE_TYPE_SUBMIT_INFO,
		nullptr,
		1, &imageacquiredsemaphores[sciindex],
		&waitstage,
		1, &primarycommandbuffers[fifindex],
		1, &submitfinishedsemaphores[fifindex]
	};
	vkQueueSubmit(genericqueue, 1, &submitinfo, submitfinishedfences[fifindex]);

	VkPresentInfoKHR presentinfo {
		VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		nullptr,
		1, &submitfinishedsemaphores[fifindex],
		1, &swapchain, &sciindex,
		nullptr
	};
	vkQueuePresentKHR(genericqueue, &presentinfo);
}

VKAPI_ATTR VkBool32 VKAPI_CALL GH::validationCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT type,
		const VkDebugUtilsMessengerCallbackDataEXT* callbackdata,
		void* userdata) {
	if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
	 || severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		std::cout << "----- Validation Error -----\n";
		std::cout << callbackdata->pMessage << std::endl;
	}
	return VK_FALSE;
}

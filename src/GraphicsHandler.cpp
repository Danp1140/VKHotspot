#include "GraphicsHandler.h"

/*
 * WindowInfo
 */

const VkPipelineStageFlags WindowInfo::defaultsubmitwaitstage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
const VkCommandBufferBeginInfo WindowInfo::primarycbbegininfo = {
	VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	nullptr,
	0,
	nullptr
};
VkCommandBufferAllocateInfo WindowInfo::cballocinfo = {
	VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
	nullptr,
	VK_NULL_HANDLE,
	VK_COMMAND_BUFFER_LEVEL_SECONDARY,
	1u
};

WindowInfo::WindowInfo() : presentationrp(VK_NULL_HANDLE), presentationfbs(nullptr) {
	int ndisplays;
	SDL_DisplayID* displays = SDL_GetDisplays(&ndisplays);
	if (ndisplays == 0 || !displays) {
		FatalError(std::string("SDL found no displays. From SDL_GetError:\n") + SDL_GetError()).raise();
	}
	const SDL_DisplayMode* displaymode = SDL_GetCurrentDisplayMode(displays[0]);
	// TODO: use SDL_GetWindowWMInfo for system-dependent window info
	SDL_free(displays);

	// TODO: ensure swapchain is properly scaled to this width/height
	sdlwindow = SDL_CreateWindow(
		"Vulkan Project", 
		displaymode->w, displaymode->h, 
		SDL_WINDOW_VULKAN);

	sdlwindowid = SDL_GetWindowID(sdlwindow);

	SDL_Vulkan_CreateSurface(
		sdlwindow, 
		GH::getInstance(),
		nullptr,
		&surface);

	VkBool32 surfacesupport;
	vkGetPhysicalDeviceSurfaceSupportKHR(
		GH::getPD(), 
		GH::getQueueFamilyIndex(), 
		surface, 
		&surfacesupport);
	if (!surfacesupport) FatalError("Window does not support surface\n").raise();

	VkSurfaceCapabilitiesKHR surfacecaps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		GH::getPD(),
		surface,
		&surfacecaps);
	numscis = surfacecaps.maxImageCount;

	const VkSwapchainCreateInfoKHR swapchaincreateinfo {
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		nullptr,
		0,
		surface,
		numscis,
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
	vkCreateSwapchainKHR(GH::getLD(), &swapchaincreateinfo, nullptr, &swapchain);

	vkGetSwapchainImagesKHR(GH::getLD(), swapchain, &numscis, nullptr);
	scimages = new ImageInfo[numscis];
	VkImage scitemp[numscis];
	vkGetSwapchainImagesKHR(GH::getLD(), swapchain, &numscis, &scitemp[0]);
	for (sciindex = 0; sciindex < numscis; sciindex++) scimages[sciindex].image = scitemp[sciindex];
	for (sciindex = 0; sciindex < numscis; sciindex++) {
		scimages[sciindex].extent = surfacecaps.currentExtent;
		scimages[sciindex].memory = VK_NULL_HANDLE;
		scimages[sciindex].format = GH_SWAPCHAIN_IMAGE_FORMAT;
		VkImageViewCreateInfo imageviewcreateinfo {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			nullptr,
			0,
			scimages[sciindex].image,
			VK_IMAGE_VIEW_TYPE_2D,
			GH_SWAPCHAIN_IMAGE_FORMAT,
			{VK_COMPONENT_SWIZZLE_IDENTITY,
			 VK_COMPONENT_SWIZZLE_IDENTITY,
			 VK_COMPONENT_SWIZZLE_IDENTITY,
			 VK_COMPONENT_SWIZZLE_IDENTITY},
			{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
		};
		vkCreateImageView(GH::getLD(), &imageviewcreateinfo, nullptr, &scimages[sciindex].view);
	}
	sciindex = 0;

	depthbuffer.extent = scimages[0].extent;
	depthbuffer.format = GH_DEPTH_BUFFER_IMAGE_FORMAT;
	depthbuffer.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	depthbuffer.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	GH::createImage(depthbuffer);

	createSyncObjects();
	createPrimaryCBs();

	rectaskvec = new std::vector<cbRecTask>[numscis];
	cballocinfo.commandPool = GH::getCommandPool();
}

WindowInfo::WindowInfo(const VkRenderPass& presrp) : WindowInfo() {
	presentationrp = presrp;
	createPresentationFBs();
}

WindowInfo::~WindowInfo() {
	vkQueueWaitIdle(GH::getGenericQueue());
	delete[] rectaskvec;
	destroyPrimaryCBs();
	destroyPresentationFBs();
	destroySyncObjects();
	GH::destroyImage(depthbuffer);
	for (sciindex = 0; sciindex < numscis; sciindex++) {
		vkDestroyImageView(GH::getLD(), scimages[sciindex].view, nullptr);
	}
	vkDestroySwapchainKHR(GH::getLD(), swapchain, nullptr);
	delete[] scimages;
	SDL_Vulkan_DestroySurface(GH::getInstance(), surface, nullptr);
	SDL_DestroyWindow(sdlwindow);
}

void WindowInfo::frameCallback() {
	vkAcquireNextImageKHR(
		GH::getLD(),
		swapchain,
		UINT64_MAX,
		imgacquiresema,
		VK_NULL_HANDLE,
		&sciindex);

	for (const cbRecTask& t : rectaskvec[sciindex]) rectasks.push(t);

	// TODO: better timeout logic
	vkWaitForFences(GH::getLD(), 1, &subfinishfences[fifindex], VK_TRUE, UINT64_MAX);
	vkResetFences(GH::getLD(), 1, &subfinishfences[fifindex]);
	processRecordingTasks(fifindex, 0, rectasks, collectinfos, secondarycbset[fifindex]);

	collectPrimaryCB();

	submitAndPresent();
}

void WindowInfo::addTask(cbRecTaskTemplate&& t) {
	if (t.type == CB_REC_TASK_TYPE_COMMAND_BUFFER) {
		for (uint8_t scii = 0; scii < numscis; scii++) {
			rectaskvec[scii].push_back(cbRecTask(t.data.func));
		}
	}
	else if (t.type == CB_REC_TASK_TYPE_RENDERPASS) {
		for (uint8_t scii = 0; scii < numscis; scii++) {
			rectaskvec[scii].push_back(cbRecTask((VkRenderPassBeginInfo){
				VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				nullptr,
				t.data.rpi.rp,
				t.data.rpi.fbs[scii],
				{{0, 0}, t.data.rpi.exts[scii]},
				t.data.rpi.nclears, t.data.rpi.clears
			}));
		}
	}
}

void WindowInfo::setPresentationRP(const VkRenderPass& presrp) {
	vkQueueWaitIdle(GH::getGenericQueue());
	presentationrp = presrp;
	destroyPresentationFBs();
	createPresentationFBs();
}

void WindowInfo::createSyncObjects() {
	VkSemaphoreCreateInfo imgacquiresemacreateinfo {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0};
	vkCreateSemaphore(GH::getLD(), &imgacquiresemacreateinfo, nullptr, &imgacquiresema);
	VkSemaphoreCreateInfo subfinishsemacreateinfo {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0};
	VkFenceCreateInfo subfinishfencecreateinfo {
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, 
		nullptr, 
		VK_FENCE_CREATE_SIGNALED_BIT
	};
	for (uint8_t fifi = 0; fifi < GH_MAX_FRAMES_IN_FLIGHT; fifi++) {
		vkCreateSemaphore(GH::getLD(), &subfinishsemacreateinfo, nullptr, &subfinishsemas[fifi]);
		vkCreateFence(GH::getLD(), &subfinishfencecreateinfo, nullptr, &subfinishfences[fifi]);
	}
}

void WindowInfo::destroySyncObjects() {
	for (uint8_t fifi = 0; fifi < GH_MAX_FRAMES_IN_FLIGHT; fifi++) {
		vkDestroyFence(GH::getLD(), subfinishfences[fifi], nullptr);
		vkDestroySemaphore(GH::getLD(), subfinishsemas[fifi], nullptr);
	}
	vkDestroySemaphore(GH::getLD(), imgacquiresema, nullptr);
}

void WindowInfo::createPresentationFBs() {
	presentationfbs = new VkFramebuffer[numscis];
	/*
	VkImageView attachments[2];
	attachments[1] = depthbuffer.view;
	VkFramebufferCreateInfo framebufferci {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		nullptr,
		0,
		presentationrp,
		2, &attachments[0],
		scimages[0].extent.width, scimages[0].extent.height, 1
	};
	for (uint8_t scii = 0; scii < numscis; scii++) {
		attachments[0] = scimages[scii].view;
		vkCreateFramebuffer(GH::getLD(), &framebufferci, nullptr, &presentationfbs[scii]);
	}
	*/
	VkFramebufferCreateInfo framebufferci {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		nullptr,
		0,
		presentationrp,
		1, nullptr,
		scimages[0].extent.width, scimages[0].extent.height, 1
	};
	for (uint8_t scii = 0; scii < numscis; scii++) {
		framebufferci.pAttachments = &scimages[scii].view;
		vkCreateFramebuffer(GH::getLD(), &framebufferci, nullptr, &presentationfbs[scii]);
	}

}

void WindowInfo::destroyPresentationFBs() {
	if (presentationfbs) {
		for (uint8_t scii = 0; scii < numscis; scii++) {
			vkDestroyFramebuffer(GH::getLD(), presentationfbs[scii], nullptr);
		}
		delete[] presentationfbs;
	}
}

void WindowInfo::createPrimaryCBs() {
	fifindex = 0u;
	VkCommandBufferAllocateInfo commandbufferai {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		nullptr,
		GH::getCommandPool(),
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		GH_MAX_FRAMES_IN_FLIGHT,
	};
	vkAllocateCommandBuffers(GH::getLD(), &commandbufferai, &primarycbs[0]);
}

void WindowInfo::destroyPrimaryCBs() {
	vkFreeCommandBuffers(GH::getLD(), GH::getCommandPool(), GH_MAX_FRAMES_IN_FLIGHT, &primarycbs[0]);
}

void WindowInfo::processRecordingTasks(
	uint8_t fifindex,
	uint8_t threadindex,
	std::queue<cbRecTask>& rectasks,
	std::queue<cbCollectInfo>& collectinfos,
	std::vector<VkCommandBuffer>& secondarycbset) {
	size_t bufferidx = 0;
	cbRecFunc recfunc;
	while (!rectasks.empty()) {
		if (rectasks.front().type == cbRecTaskType::CB_REC_TASK_TYPE_RENDERPASS) {
			collectinfos.push(cbCollectInfo(rectasks.front().data.rpbi));
			rectasks.pop();
			continue;
		}
		if (rectasks.front().type == cbRecTaskType::CB_REC_TASK_TYPE_DEPENDENCY) {
			collectinfos.push(cbCollectInfo(rectasks.front().data.di));
			rectasks.pop();
			continue;
		}
		recfunc = rectasks.front().data.func;
		rectasks.pop();
		if (bufferidx == secondarycbset.size()) {
			if (bufferidx == secondarycbset.size()) {
				secondarycbset.push_back(VK_NULL_HANDLE);
				vkAllocateCommandBuffers(
					GH::getLD(),
					&cballocinfo,
					&secondarycbset.back());
			}
		}
		collectinfos.push(cbCollectInfo(secondarycbset[bufferidx]));
		recfunc(secondarycbset[bufferidx]);
	}
}

void WindowInfo::collectPrimaryCB() {
	vkBeginCommandBuffer(primarycbs[fifindex], &primarycbbegininfo);
	bool inrp = false;
	while (!collectinfos.empty()) {
		if (collectinfos.front().type == cbCollectInfo::cbCollectInfoType::CB_COLLECT_INFO_TYPE_COMMAND_BUFFER) {
			vkCmdExecuteCommands(
				primarycbs[fifindex],
				1,
				&collectinfos.front().data.cmdbuf);
		} 
		else if (collectinfos.front().type == cbCollectInfo::cbCollectInfoType::CB_COLLECT_INFO_TYPE_RENDERPASS) {
			// ending an rp w/o starting another is done by passing a renderpassbi 
			// as if to begin, but with a null handle as renderpass
			if (inrp) vkCmdEndRenderPass(primarycbs[fifindex]);
			else inrp = true;
			if (collectinfos.front().data.rpbi.renderPass != VK_NULL_HANDLE) {
				vkCmdBeginRenderPass(
					primarycbs[fifindex],
					&collectinfos.front().data.rpbi,
					VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			}
			else inrp = false;
		}
		else if (collectinfos.front().type == cbCollectInfo::cbCollectInfoType::CB_COLLECT_INFO_TYPE_DEPENDENCY) {
			if (collectinfos.front().data.di.imageMemoryBarrierCount) {
				FatalError("Collection of dependencies/pipeline barriers not yet supported\n").raise();
				/*
				if (inrp) {
					vkCmdEndRenderPass(primarycbs[fifindex]);
					inrp = false;
				}

				// i feel like we should be able to use vkCmdPipelineBarrier2(KHR), but it crashed when I tried...
				GraphicsHandler::pipelineBarrierFromKHR(secondarybuffers.front().data.di);
				delete[] secondarybuffers.front().data.di.pImageMemoryBarriers;
				*/
			}
		}
		collectinfos.pop();
	}
	if (inrp) vkCmdEndRenderPass(primarycbs[fifindex]);
	vkEndCommandBuffer(primarycbs[fifindex]);
}

void WindowInfo::submitAndPresent() {
	submitinfo = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO,
		nullptr,
		1, &imgacquiresema,
		&defaultsubmitwaitstage,
		1, &primarycbs[fifindex],
		1, &subfinishsemas[fifindex]
	};
	vkQueueSubmit(GH::getGenericQueue(), 1, &submitinfo, subfinishfences[fifindex]);

	presentinfo = {
		VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		nullptr,
		1, &subfinishsemas[fifindex],
		1, &swapchain, &sciindex,
		nullptr
	};
	vkQueuePresentKHR(GH::getGenericQueue(), &presentinfo);

	fifindex++;
	if (fifindex == GH_MAX_FRAMES_IN_FLIGHT) fifindex = 0;
}

/*
 * GraphicsHandler
 */

VkInstance GH::instance = VK_NULL_HANDLE;
VkDevice GH::logicaldevice = VK_NULL_HANDLE;
VkPhysicalDevice GH::physicaldevice = VK_NULL_HANDLE;
VkQueue GH::genericqueue = VK_NULL_HANDLE;
uint8_t GH::queuefamilyindex = 0xff;
VkRenderPass GH::primaryrenderpass = VK_NULL_HANDLE;
VkCommandPool GH::commandpool = VK_NULL_HANDLE;
VkCommandBuffer GH::interimcb = VK_NULL_HANDLE;
const VkClearValue GH::primaryclears[2] = {{0.1, 0.1, 0.1, 1.}, 0.};
VkDescriptorPool GH::descriptorpool = VK_NULL_HANDLE;
VkSampler GH::nearestsampler = VK_NULL_HANDLE;

GH::GH() {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		FatalError(
			std::string("SDL3 Initialization Failed! From SDL_GetError():\n") 
			 + SDL_GetError()).raise();
	}
	initVulkanInstance();
	initDebug();
	initDevicesAndQueues();
	initRenderpasses();
	initCommandPools();
	initSamplers();
	initDescriptorPoolsAndSetLayouts();
}

GH::~GH() {
	vkQueueWaitIdle(genericqueue);
	terminateDescriptorPoolsAndSetLayouts();
	terminateSamplers();
	terminateCommandPools();
	terminateRenderpasses();
	terminateDevicesAndQueues();
	terminateDebug();
	terminateVulkanInstance();
	SDL_Quit();
}

void GH::initVulkanInstance() {
	VkApplicationInfo appinfo {
		VK_STRUCTURE_TYPE_APPLICATION_INFO,
		nullptr,
		"WaveBox",
		VK_MAKE_VERSION(1, 0, 0),
		"Jet",
		VK_MAKE_VERSION(1, 0, 0),
		VK_MAKE_API_VERSION(0, 1, 0, 0)
	};

	/*
	 * SDL extensions should handle surface and metal surface, but its unclear if 
	 * surface should be included regardless of SDL extension response.
	 */
	uint32_t nsdlexts;
	char const * const * sdlexts = SDL_Vulkan_GetInstanceExtensions(&nsdlexts);
	const char* layers[1] {"VK_LAYER_KHRONOS_validation"};
	uint32_t nallowedexts;
	vkEnumerateInstanceExtensionProperties(nullptr, &nallowedexts, nullptr);
	VkExtensionProperties allowedexts[nallowedexts];
	vkEnumerateInstanceExtensionProperties(nullptr, &nallowedexts, &allowedexts[0]);
	std::vector<const char*> extensions = {
		"VK_MVK_macos_surface", // only required on OSX, TODO: add os build maacro check
		"VK_KHR_get_physical_device_properties2", // unclear why this is needeed
		"VK_EXT_debug_utils" // for val layers, not needed in final compilation
	};
	bool allowed;
	for (uint32_t i = 0; i < nsdlexts; i++) {
		allowed = false;
		for (uint32_t j = 0; j < nallowedexts; j++) {
			if (strcmp(sdlexts[i], &allowedexts[j].extensionName[0]) == 0) {
				allowed = true;
				break;
			}
		}
		if (!allowed) {
			WarningError(std::string("SDL requested unsupported instance extension ") + sdlexts[i]).raise();
		}
		else {
			extensions.push_back(sdlexts[i]);
		}
	}
	VkInstanceCreateInfo instancecreateinfo {
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		nullptr,
		0,
		&appinfo,
		1, &layers[0],
		static_cast<uint32_t>(extensions.size()), extensions.data() 
	};
	FatalError("Vulkan instance creation error\n")
		.vkCatch(vkCreateInstance(&instancecreateinfo, nullptr, &instance));
}

void GH::terminateVulkanInstance() {
	vkDestroyInstance(instance, nullptr);
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
	queuefamilyindex = 0;
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

void GH::initCommandPools() {
	VkCommandPoolCreateInfo commandpoolci {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		nullptr,
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		queuefamilyindex
	};
	vkCreateCommandPool(logicaldevice, &commandpoolci, nullptr, &commandpool);

	VkCommandBufferAllocateInfo cballocinfo {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		nullptr,
		commandpool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		1
	};
	vkAllocateCommandBuffers(
		logicaldevice,
		&cballocinfo,
		&interimcb);
}

void GH::terminateCommandPools() {
	vkFreeCommandBuffers(
			logicaldevice, 
			commandpool,
			1, &interimcb);

	vkDestroyCommandPool(logicaldevice, commandpool, nullptr);
}

void GH::initSamplers() {
	VkSamplerCreateInfo samplerci {
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		nullptr,
		0,
		VK_FILTER_NEAREST, VK_FILTER_NEAREST,
		VK_SAMPLER_MIPMAP_MODE_NEAREST,
		VK_SAMPLER_ADDRESS_MODE_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_REPEAT,
		0.,
		VK_FALSE, 0.,
		VK_FALSE, VK_COMPARE_OP_NEVER,
		0., 0., 
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
		VK_FALSE
	};
	vkCreateSampler(logicaldevice, &samplerci, nullptr, &nearestsampler);
}

void GH::terminateSamplers() {
	vkDestroySampler(logicaldevice, nearestsampler, nullptr);
}

void GH::initDescriptorPoolsAndSetLayouts() {
	VkDescriptorPoolSize poolsizes[1] {
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
	};
	VkDescriptorPoolCreateInfo descriptorpoolci {
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		nullptr,
		0,
		1,
		1, &poolsizes[0] 
	};
	vkCreateDescriptorPool(logicaldevice, &descriptorpoolci, nullptr, &descriptorpool);
}

void GH::terminateDescriptorPoolsAndSetLayouts() {
	vkDestroyDescriptorPool(logicaldevice, descriptorpool, nullptr);
}

void GH::createRenderPass(
	VkRenderPass& rp,
	uint8_t numattachments,
	VkAttachmentDescription* attachmentdescs,
	VkAttachmentReference* colorattachmentrefs,
	VkAttachmentReference* depthattachmentref) {
	VkSubpassDescription primarysubpassdescription {
		0,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		0, nullptr,
		static_cast<uint32_t>(numattachments - (depthattachmentref ? 1 : 0)), 
		colorattachmentrefs, nullptr, depthattachmentref,
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
	VkRenderPassCreateInfo rpcreateinfo {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		nullptr,
		0,
		numattachments, &attachmentdescs[0],
		1, &primarysubpassdescription,
		1, &subpassdependency
	};
	vkCreateRenderPass(logicaldevice, &rpcreateinfo, nullptr, &rp);
}

void GH::destroyRenderPass(VkRenderPass& rp) {
	vkDestroyRenderPass(logicaldevice, rp, nullptr);
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
		std::string tempstr = std::string(SHADER_DIRECTORY)
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

	if (pi.extent.width == 0 || pi.extent.height == 0) {
		WarningError("GH::createPipeline given a PipelineInfo struct with zero height or width\n").raise();
		return;
	}

	uint32_t numshaderstages = 0;
	char* shaderfilepaths[NUM_SHADER_STAGES_SUPPORTED];
	std::string temp;
	for (uint8_t i = 0; i < NUM_SHADER_STAGES_SUPPORTED; i++) {
		if (supportedshaderstages[i] & pi.stages) {
			temp = std::string(SHADER_DIRECTORY)
				.append(pi.shaderfilepathprefix)
				.append(shaderstagestrs[i])
				.append(".spv");
			shaderfilepaths[numshaderstages] = new char[temp.length() + 1]; // adding 1 for null terminator
			temp.copy(shaderfilepaths[numshaderstages], temp.length());
			shaderfilepaths[numshaderstages][temp.length()] = '\0';
			numshaderstages++;
		}
	}
	// TODO: why are these allocated with new???
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
	const ImageInfo& i,
	VkDeviceMemory& memory) {
	VkMemoryRequirements memreqs;
	if (buffer != VK_NULL_HANDLE) vkGetBufferMemoryRequirements(logicaldevice, buffer, &memreqs);
	else vkGetImageMemoryRequirements(logicaldevice, i.image, &memreqs);
	VkPhysicalDeviceMemoryProperties physicaldevicememprops;
	vkGetPhysicalDeviceMemoryProperties(physicaldevice, &physicaldevicememprops);
	// TODO: BufferInfo struct so we don't hard-code this
	const VkMemoryPropertyFlags memprops = buffer ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : i.memprops;
	uint32_t finalmemindex = -1u;
	for (uint32_t memindex = 0; memindex < physicaldevicememprops.memoryTypeCount; memindex++) {
		if (memreqs.memoryTypeBits & (1 << memindex)
		    && (physicaldevicememprops.memoryTypes[memindex].propertyFlags & memprops) == memprops) {
			finalmemindex = memindex;
			break;
		}
	}
	if (finalmemindex == -1u) {
		FatalError("Couldn't find appropriate memory to allocate\n"
				"Likely to be a tiling compatability issue").raise();
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

void GH::createDS(const PipelineInfo& p, VkDescriptorSet& ds) {
	VkDescriptorSetAllocateInfo allocinfo {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		nullptr,
		descriptorpool,
		1, &p.dsl
	};
	vkAllocateDescriptorSets(logicaldevice, &allocinfo, &ds);
}

void GH::updateDS(
	const VkDescriptorSet& ds, 
	uint32_t i,
	VkDescriptorType t,
	VkDescriptorImageInfo ii,
	VkDescriptorBufferInfo bi) {
	VkWriteDescriptorSet write {
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		ds,
		i,
		0,
		1,
		t,
		&ii, &bi, nullptr
	};
	vkUpdateDescriptorSets(logicaldevice, 1, &write, 0, nullptr);
}

void GH::updateWholeDS(
	const VkDescriptorSet& ds, 
	std::vector<VkDescriptorType>&& t,
	std::vector<VkDescriptorImageInfo>&& ii,
	std::vector<VkDescriptorBufferInfo>&& bi) {
	VkWriteDescriptorSet writes[t.size()];
	for (uint32_t i = 0; i < t.size(); i++) {
		writes[i] = {
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			nullptr,
			ds,
			i,
			0,
			1,
			t[i],
			&ii[i], &bi[i], nullptr
		};
	}
	vkUpdateDescriptorSets(logicaldevice, t.size(), &writes[0], 0, nullptr);
}

void GH::createImage(ImageInfo& i) {
	// stolen from other code i wrote, unsure why tiling should be determined this way
	// in reality this is likely to be system-dependent requiring some device capability querying
	i.tiling = i.format != VK_FORMAT_D32_SFLOAT ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
	VkImageLayout finallayout = i.layout;
	i.layout = VK_IMAGE_LAYOUT_UNDEFINED;
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
		i.tiling,
		i.usage,
		VK_SHARING_MODE_EXCLUSIVE,
		0, nullptr,
		i.layout
	};
	vkCreateImage(logicaldevice, &imageci, nullptr, &i.image);

	allocateDeviceMemory(
		VK_NULL_HANDLE, 
		i,
		i.memory);
	vkBindImageMemory(logicaldevice, i.image, i.memory, 0);

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
		i.getDefaultSubresourceRange()
	};
	vkCreateImageView(logicaldevice, &imageviewci, nullptr, &i.view);

	transitionImageLayout(i, finallayout);
}

void GH::destroyImage(ImageInfo& i) {
	vkDestroyImageView(logicaldevice, i.view, nullptr);
	freeDeviceMemory(i.memory);
	vkDestroyImage(logicaldevice, i.image, nullptr);
}

void GH::updateImage(ImageInfo& i, void* src) {
	// TODO: handle image transitions when required
	// TODO: handle device local images using a staging buffer
	bool querysubresource = (i.tiling == VK_IMAGE_TILING_LINEAR);
	VkSubresourceLayout subresourcelayout;
	VkImageSubresource imgsubresource = i.getDefaultSubresource();
	if (querysubresource) {
		vkGetImageSubresourceLayout(
			logicaldevice, 
			i.image, 
			&imgsubresource, 
			&subresourcelayout);
	}
	void* dst;
	vkMapMemory(logicaldevice,
		i.memory,
		0,
		VK_WHOLE_SIZE,
		0,
		&dst);
	VkDeviceSize pitch;
	if (querysubresource) pitch = subresourcelayout.rowPitch;
	else pitch = i.extent.width * i.getPixelSize();
	char* dstscan = reinterpret_cast<char*>(dst), * srcscan = reinterpret_cast<char*>(src);
	for (uint32_t x = 0; x < i.extent.height; x++) {
		memcpy(reinterpret_cast<void*>(dstscan), 
			reinterpret_cast<void*>(srcscan),
			i.extent.width * i.getPixelSize());
		dstscan += pitch;
		srcscan += i.extent.width * i.getPixelSize();
	}
	vkUnmapMemory(logicaldevice, i.memory);
}

void GH::transitionImageLayout(ImageInfo& i, VkImageLayout newlayout) {
	VkImageMemoryBarrier imgmembarrier {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		nullptr,
		0,
		0,
		i.layout,
		newlayout,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		i.image,
		i.getDefaultSubresourceRange()
	};
	VkPipelineStageFlags srcmask, dstmask;
	switch (i.layout) {
		case VK_IMAGE_LAYOUT_UNDEFINED:
			imgmembarrier.srcAccessMask = 0;
			srcmask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			imgmembarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			srcmask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			imgmembarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			srcmask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			imgmembarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT 
							| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			srcmask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			imgmembarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			srcmask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;
		default:
			WarningError("unknown initial layout for img transition").raise();
	}
	switch (newlayout) {
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			imgmembarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			dstmask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			imgmembarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dstmask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			imgmembarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | 
							VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dstmask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			break;
		case VK_IMAGE_LAYOUT_GENERAL:
			imgmembarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			dstmask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
			imgmembarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | 
							VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dstmask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		default:
			WarningError("unknown final layout for img transition").raise();
	}

	vkBeginCommandBuffer(interimcb, &interimcbbegininfo);
	vkCmdPipelineBarrier(
		interimcb, 
		srcmask, dstmask, 
		0, 
		0, nullptr, 
		0, nullptr,
		1, &imgmembarrier);
	vkEndCommandBuffer(interimcb);
	VkSubmitInfo subinfo {
		VK_STRUCTURE_TYPE_SUBMIT_INFO,
		nullptr,
		0,
		nullptr,
		nullptr,
		1,
		&interimcb,
		0,
		nullptr
	};
	vkQueueSubmit(genericqueue, 1, &subinfo, VK_NULL_HANDLE);
	i.layout = newlayout;
	vkQueueWaitIdle(genericqueue);
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

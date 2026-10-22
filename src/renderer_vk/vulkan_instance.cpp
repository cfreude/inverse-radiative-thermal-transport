#include <tamashii/renderer_vk/vulkan_instance.hpp>
#include <tamashii/engine/common/common.hpp>
#include <tamashii/engine/common/vars.hpp>
#include <tamashii/engine/platform/window.hpp>
#include <spdlog/fmt/bundled/color.h>
#include "gui/imgui_vulkan.hpp"

RVK_USE_NAMESPACE
T_USE_NAMESPACE

namespace {
	VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		if (VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT == messageSeverity) spdlog::error("{}", pCallbackData->pMessage);
		else if (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT == messageSeverity) spdlog::warn("{}", pCallbackData->pMessage);
		else if (VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT == messageSeverity) spdlog::info("{}", pCallbackData->pMessage);
		else spdlog::debug("{}", pCallbackData->pMessage);
		if (pCallbackData->cmdBufLabelCount)
		{
			for (uint32_t i = 0; i < pCallbackData->cmdBufLabelCount; ++i)
			{
				const VkDebugUtilsLabelEXT* label = &pCallbackData->pCmdBufLabels[i];
				spdlog::warn("{} ~", label->pLabelName);
			}
		}

		if (pCallbackData->objectCount)
		{
			for (uint32_t i = 0; i < pCallbackData->objectCount; ++i)
			{
				const VkDebugUtilsObjectNameInfoEXT* obj = &pCallbackData->pObjects[i];
				spdlog::warn("--- {} {}", obj->pObjectName, static_cast<int32_t>(obj->objectType));
			}
		}
#ifdef VALIDATION_LAYER_THROW_RUNTIME_ERROR
		throw std::runtime_error("Vulkan Validation Layer");
#endif // VALIDATION_ERROR_RUNTIME_ERROR
		return VK_FALSE;
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL debugCallbackPrintf(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		// debugMessageIdName = "UNASSIGNED-DEBUG-PRINTF"; debugMessageIdNumber = 0x92394c89;
		if (!pCallbackData->pMessageIdName) return VK_FALSE;
		if (std::string(pCallbackData->pMessageIdName) != "UNASSIGNED-DEBUG-PRINTF") return VK_FALSE;
		std::string str = pCallbackData->pMessage;
		const std::string findStr = "0x92394c89 | ";
		str.replace(0, str.find(findStr) + findStr.size(), 
			fmt::format(fmt::fg(fmt::terminal_color::cyan), "shader "));
		spdlog::debug("{}", str);
		return VK_FALSE;
	}
}

VulkanInstance::VulkanInstance() : mVendor(), mLib(nullptr), mInstance(nullptr), mDevice(nullptr), mSwapchainData(),
                                   mDebugCallback(nullptr),
                                   mDebugCallbackPrintf(nullptr), mImGuiPool(nullptr)
{
	rvk::Logger::setInfoCallback([](const std::string& aString) { spdlog::info(aString); });
	rvk::Logger::setDebugCallback([](const std::string& aString) { spdlog::debug(aString); });
	rvk::Logger::setWarningCallback([](const std::string& aString) { spdlog::warn(aString); });
	rvk::Logger::setCriticalCallback([](const std::string& aString) { spdlog::critical(aString); });
	rvk::Logger::setErrorCallback([](const std::string& aString) { spdlog::error(aString); });

	mInstanceExtensions = {
#ifndef NDEBUG
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
		VK_KHR_SURFACE_EXTENSION_NAME,
#if defined( _WIN32 )
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(__APPLE__)
		VK_MVK_MACOS_SURFACE_EXTENSION_NAME
#elif defined( __linux__ )
#if defined( VK_USE_PLATFORM_WAYLAND_KHR )
		VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
#elif defined( VK_USE_PLATFORM_XLIB_KHR )
		VK_KHR_XLIB_SURFACE_EXTENSION_NAME
#elif defined( VK_USE_PLATFORM_XCB_KHR )
		VK_KHR_XCB_SURFACE_EXTENSION_NAME
#endif
#endif
	};

	mValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};
}

void VulkanInstance::init(const std::vector<tamashii::Window*>& aWindows)
{
	const std::vector<const char*> defaultDeviceExtensions{
#if defined(__APPLE__)
		//VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
#endif
		VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
		VK_KHR_MAINTENANCE3_EXTENSION_NAME,
		VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME,
#ifdef VK_KHR_swapchain
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#endif
		//#ifdef VK_NVX_binary_import
		//		VK_NVX_BINARY_IMPORT_EXTENSION_NAME,
		//#endif
		VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
		VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
		VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME,
		VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
#if defined(WIN32)
		VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
		VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
		VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME,
#else
		VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
		VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
		VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME,
#endif
		// for GLSL_EXT_debug_printf
		#ifdef VK_KHR_shader_non_semantic_info
				VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME
		#endif
		#ifndef NDEBUG
				//VK_EXT_DEBUG_MARKER_EXTENSION_NAME
		#endif
	};

	const std::vector<const char*> rtDeviceExtensions{
#ifdef VK_KHR_acceleration_structure
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
#endif
#ifdef VK_KHR_ray_tracing_pipeline
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
#endif
#ifdef VK_KHR_ray_query
		VK_KHR_RAY_QUERY_EXTENSION_NAME,
#endif
		// required by VK_KHR_acceleration_structure
#ifdef VK_KHR_buffer_device_address
		VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
#endif
#ifdef VK_KHR_deferred_host_operations
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
#endif
		// required by VK_KHR_ray_tracing_pipeline
#ifdef VK_KHR_spirv_1_4
		VK_KHR_SPIRV_1_4_EXTENSION_NAME,
#endif
		// required by VK_KHR_spirv_1_4
#ifdef VK_KHR_shader_float_controls
		VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
#endif
	};

	mMainThreadId = std::this_thread::get_id();
	// load lib
	if (!((mLib = InstanceManager::loadLibrary()))) {
		spdlog::error("...could not initialize Vulkan");
		exit(1);
	}
	spdlog::info("...loading entry function");
	const PFN_vkGetInstanceProcAddr func = InstanceManager::loadEntryFunction(mLib);
	if (!func) {
		spdlog::error("...could not load entry function");
		exit(1);
	}
	InstanceManager& instanceManager = InstanceManager::getInstance();
	instanceManager.init(func);

	// instance
	const bool validationLayerAvailable = instanceManager.areLayersAvailable(mValidationLayers);
	if (!validationLayerAvailable) mValidationLayers.clear();
	void* pNext = nullptr;
#ifndef NDEBUG
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT  };
	debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugCreateInfo.pfnUserCallback = &debugCallback;
	std::vector enabledValidationFeatures{ VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT };
	VkValidationFeaturesEXT validationFeatures = {};
	validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
	validationFeatures.enabledValidationFeatureCount = static_cast<uint32_t>(enabledValidationFeatures.size());
	validationFeatures.pEnabledValidationFeatures = enabledValidationFeatures.data();

	debugCreateInfo.pNext = &validationFeatures;
	if (validationLayerAvailable) pNext = &validationFeatures;
#endif
	mInstance = instanceManager.createInstance(mInstanceExtensions, mValidationLayers, pNext);
#ifndef NDEBUG
	if (validationLayerAvailable) setupDebugCallback();
#endif

	// surfaces
	uint32_t count = 0;
	for(tamashii::Window* window : aWindows) {
		mSurfaces.emplace_back("window_" + std::to_string(count));
		mInstance->registerSurface(mSurfaces.back(), window->getWindowHandle(), window->getInstanceHandle());
		count++;
	}

	// device
	PhysicalDevice* d = mInstance->findPhysicalDevice(defaultDeviceExtensions);
	if(!d) {
		spdlog::error("No suitable Device found!");
		Common::getInstance().shutdown();
	}

	// check rt extensions
	mDeviceExtensions.reserve(defaultDeviceExtensions.size() + rtDeviceExtensions.size());
	mDeviceExtensions.insert(mDeviceExtensions.end(), defaultDeviceExtensions.begin(), defaultDeviceExtensions.end());
	bool rtAvailable = true;
	for(const char* ext : rtDeviceExtensions)
	{
		if (!d->isExtensionAvailable(ext)) rtAvailable = false;
	}
	if (rtAvailable) mDeviceExtensions.insert(mDeviceExtensions.end(), rtDeviceExtensions.begin(), rtDeviceExtensions.end());
	else spdlog::warn("Vulkan: Ray Tracing not supported by device");

	// queues
	uint32_t qflags = PhysicalDevice::Queue::GRAPHICS | PhysicalDevice::Queue::COMPUTE | PhysicalDevice::Queue::TRANSFER;
	int idx = d->getQueueFamilyIndex(qflags, mSurfaces);
	count = d->getAvailableQueueCount(idx);

	// activate features
	VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexingFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT };
	VkPhysicalDeviceShaderAtomicFloatFeaturesEXT floatFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT };
	indexingFeatures.pNext = &floatFeatures;
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR pipelineFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
	floatFeatures.pNext = &pipelineFeatures;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
	pipelineFeatures.pNext = &asFeatures;
	VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeature = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
	asFeatures.pNext = &bufferDeviceAddressFeature;
	VkPhysicalDeviceFeatures2 device_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR };
	device_features.pNext = &indexingFeatures;
	mInstance->vk.GetPhysicalDeviceFeatures2(d->getHandle(), &device_features);

#define CHECK_FEATURE(struct, feat) if(!struct.feat) spdlog::warn("Vulkan: Feature '" #feat "' not supported by device");
	CHECK_FEATURE(device_features.features, fillModeNonSolid)
	CHECK_FEATURE(device_features.features, multiDrawIndirect)
	CHECK_FEATURE(device_features.features, drawIndirectFirstInstance)
	CHECK_FEATURE(device_features.features, shaderClipDistance)
	CHECK_FEATURE(device_features.features, geometryShader)
	CHECK_FEATURE(device_features.features, shaderFloat64)
	CHECK_FEATURE(device_features.features, shaderInt64)
	CHECK_FEATURE(indexingFeatures, runtimeDescriptorArray)
	CHECK_FEATURE(indexingFeatures, descriptorBindingVariableDescriptorCount)
	CHECK_FEATURE(indexingFeatures, descriptorBindingPartiallyBound)
	CHECK_FEATURE(floatFeatures, shaderBufferFloat32AtomicAdd)
	CHECK_FEATURE(floatFeatures, shaderBufferFloat64AtomicAdd)
	if (rtAvailable) {
		CHECK_FEATURE(pipelineFeatures, rayTracingPipeline)
		CHECK_FEATURE(pipelineFeatures, rayTracingPipelineTraceRaysIndirect)
		CHECK_FEATURE(pipelineFeatures, rayTraversalPrimitiveCulling)
		CHECK_FEATURE(asFeatures, accelerationStructure)
		CHECK_FEATURE(bufferDeviceAddressFeature, bufferDeviceAddress)
	}
#undef CHECK_FEATURE

	mDevice = d->createLogicalDevice(mDeviceExtensions, { { 0, {1.0f, 1.0f, 1.0f} } }, &device_features);
	VkPhysicalDeviceProperties deviceProperties = mDevice->getPhysicalDevice()->getProperties();
	if (deviceProperties.vendorID == 0x1002) mVendor = Vendor::AMD;
	else if (deviceProperties.vendorID == 0x10DE) mVendor = Vendor::NVIDIA;
	else if (deviceProperties.vendorID == 0x8086) mVendor = Vendor::INTEL;

	const bool headless = tamashii::var::headless.getBool();
	// emulate swapchain
	if(headless)
	{
		mSwapchainData.mId = "emulated";
		mSwapchainData.mImages.resize(2);
	}
	// swapchain
	else {
		Swapchain* sc = mDevice->createSwapchain(mSurfaces.front());
		sc->setImageCount(3);

		if (tamashii::var::vsync.getBool() && sc->checkPresentMode(VK_PRESENT_MODE_FIFO_KHR)) sc->setPresentMode(VK_PRESENT_MODE_FIFO_KHR);
		else if (sc->checkPresentMode(VK_PRESENT_MODE_MAILBOX_KHR)) sc->setPresentMode(VK_PRESENT_MODE_MAILBOX_KHR);
		else if (sc->checkPresentMode(VK_PRESENT_MODE_IMMEDIATE_KHR)) sc->setPresentMode(VK_PRESENT_MODE_IMMEDIATE_KHR);

		sc->setSurfaceFormat({ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
		const VkSurfaceCapabilitiesKHR& surfaceCapabilities = sc->getSurfaceCapabilities();
		const std::vector depthFormats = { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT };
		for (const VkFormat& f : depthFormats)
		{
			if (sc->checkDepthFormat(f))
			{
				sc->useDepth(f);
				break;
			}
		}
		sc->finish();
		spdlog::info("...swapchain created ({}x{})", sc->getExtent().width, sc->getExtent().height);
		mSwapchainData.mWindow = aWindows.front();
		mSwapchainData.mId = mSurfaces.front();
		mSwapchainData.mSwapchain = sc;
		mSwapchainData.mImages.resize(sc->getImageCount());
	}

	{
		mSwapchainData.mCommandPool = mDevice->createCommandPool(0, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		for (SwapchainImageData& imgData : mSwapchainData.mImages)
		{
			if (mSwapchainData.mSwapchain) {
				imgData.mNextImageAvailableSemaphores = mDevice->createSemaphore();
				imgData.mRenderFinishedSemaphores = mDevice->createSemaphore();
			}
			imgData.mInFlightFences = mDevice->createFence(VK_FENCE_CREATE_SIGNALED_BIT);
			imgData.mCommandBuffer = mSwapchainData.mCommandPool->allocCommandBuffers(1).front();
		}
	}
	// stc for two threads
	mCommandPools.reserve(2);
	mCommandPools.push_back(mDevice->createCommandPool(0));
	mCommandPools.push_back(mDevice->createCommandPool(0));

	// transition swapchain
	if (!headless) {
		CommandBuffer* cb = mSwapchainData.mImages.front().mCommandBuffer;
		cb->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		//for (auto& p : mSwapchainsData)
		//{
			for (SwapchainImageData& imgData : mSwapchainData.mImages)
			{
				imgData.mCommandBuffer = mSwapchainData.mCommandPool->allocCommandBuffers(1).front();
			}
			for (rvk::Image& img : mSwapchainData.mSwapchain->getImages()) img.CMD_TransitionImage(cb, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			for (rvk::Image& img : mSwapchainData.mSwapchain->getDepthImages()) img.CMD_TransitionImage(cb, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

		//}
		cb->end();
		mDevice->getQueue(0, 0)->submitCommandBuffers({ cb });
		mDevice->getQueue(0, 0)->waitIdle();
	}
	setDefaultState();
}

void VulkanInstance::initImGui()
{
	Swapchain* sc = mSwapchainData.mSwapchain;
	mImGuiPool = ImGui_Vulkan_Impl::setup(mCommandPools[0], mDevice->getQueue(0, 0), sc);
}

void VulkanInstance::printDeviceInfos() const
{
	uint32_t count = 0;
	const char* vendorName = "unknown";
	VkPhysicalDeviceProperties deviceProperties = mDevice->getPhysicalDevice()->getProperties();
	if (deviceProperties.vendorID == 0x1002) {
		vendorName = "Advanced Micro Devices, Inc.";
	}
	else if (deviceProperties.vendorID == 0x10DE) {
		vendorName = "NVIDIA Corporation";
	}
	else if (deviceProperties.vendorID == 0x8086) {
		vendorName = "Intel Corporation";
	}
	uint32_t major = VK_VERSION_MAJOR(deviceProperties.apiVersion);
	uint32_t minor = VK_VERSION_MINOR(deviceProperties.apiVersion);
	uint32_t patch = VK_VERSION_PATCH(deviceProperties.apiVersion);

	spdlog::info("Device {}", count);
	spdlog::info("\tVendor: {}", vendorName);
	spdlog::info("\tGPU: {}", deviceProperties.deviceName);
	spdlog::info("\tVulkan Version: {}.{}.{}", major, minor, patch);
	count++;
	

	// print device extensions, but the output amount is way to high therefore we leave it out
	/*uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(vk.physical_device, NULL, &extensionCount, NULL);
	VkExtensionProperties* extensions = new VkExtensionProperties[extensionCount];
	vkEnumerateDeviceExtensionProperties(vk.physical_device, NULL, &extensionCount, &extensions[0]);
	char extensions_string[10 * 2048];
	uint32_t offset = 0;
	for (uint32_t i = 0; i < extensionCount; i++) {
		strncpy(extensions_string + offset, (const char*)extensions[i].extensionName, strlen(extensions[i].extensionName) + 1);
		offset += strlen(extensions[i].extensionName);
		strncpy(extensions_string + offset, " ", 2);
		offset += 1;
	}
	spdlog::info("Extentions: {}", extensions_string);
	free(extensions);*/
}

void VulkanInstance::destroyImGui()
{
	ImGui_Vulkan_Impl::destroy(mDevice, mImGuiPool);
}

void VulkanInstance::destroy()
{
	InstanceManager& instanceManager = InstanceManager::getInstance();
	instanceManager.destroyInstance(mInstance);
	InstanceManager::closeLibrary(mLib);
}

void VulkanInstance::setResizeCallback(const std::function<void(int, int, rvk::Swapchain*)>& aCallback)
{
	mCallbackSwapchainResize = aCallback;
}

void VulkanInstance::recreateSwapchain(const uint32_t aWidth, const uint32_t aHeight)
{
	SwapchainData& sd = mSwapchainData;
	//const std::lock_guard lock(sd.mWindow->resizeMutex());
	mDevice->waitIdle();
	int width, height;
	mSwapchainData.mSwapchain->recreateSwapchain();
	SingleTimeCommand stc(mSwapchainData.mCommandPool, mDevice->getQueue(0, 0));
	//vk.callbackSwapchainResize(width, height);
	stc.begin();
	for (rvk::Image& img : mSwapchainData.mSwapchain->getImages()) img.CMD_TransitionImage(stc.buffer(), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	for (rvk::Image& img : mSwapchainData.mSwapchain->getDepthImages()) img.CMD_TransitionImage(stc.buffer(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	stc.end();
}

void VulkanInstance::beginFrame()
{
	SwapchainData& sd = mSwapchainData;
	const bool head = sd.mSwapchain;
	
	// if headless -> ignore image
	if (head) {
		sd.mSwapchain->acquireNextImage(sd.mImages[sd.mCurrentImageIndex].mNextImageAvailableSemaphores);
		sd.mCurrentImageIndex = sd.mSwapchain->getCurrentIndex();
		sd.mPreviousImageIndex = sd.mSwapchain->getPreviousIndex();
	}
	else {
		const uint32_t imageCount = static_cast<uint32_t>(sd.mImages.size());
		sd.mPreviousImageIndex = sd.mCurrentImageIndex;
		sd.mCurrentImageIndex = (sd.mCurrentImageIndex + 1) % imageCount;
	}
	
	// we want to wait until the commands from the current image have completed execution
	const SwapchainImageData& imageData = sd.mImages[sd.mCurrentImageIndex];
	while (VK_TIMEOUT == imageData.mInFlightFences->wait()) {}
	imageData.mInFlightFences->reset();

	// destroy old pipelines
	//cleanup(currentImageIndex);
	imageData.mCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
}

void VulkanInstance::endFrame()
{
	SwapchainData& sd = mSwapchainData;
	const bool head = sd.mSwapchain;
	const SwapchainImageData& prevImageData = sd.mImages[sd.mPreviousImageIndex];
	const SwapchainImageData& currImageData = sd.mImages[sd.mCurrentImageIndex];

	currImageData.mCommandBuffer->end();
	if (head) {
		// if swapchain -> wait until image is ready to process
		const std::vector<std::pair<Semaphore*, VkPipelineStageFlags>> waitSemaphore = {
			{ prevImageData.mNextImageAvailableSemaphores, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT} };
		const std::vector signalSemaphores{ currImageData.mRenderFinishedSemaphores };
		mDevice->getQueue(0, 0)->submitCommandBuffers({ currImageData.mCommandBuffer },
			currImageData.mInFlightFences, waitSemaphore, signalSemaphores);
		// if swapchain -> also wait until image is ready to display
		const VkResult r = mDevice->getQueue(0, 0)->present({ sd.mSwapchain }, signalSemaphores);
		if (r == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapchain(0, 0);
			const VkExtent2D extent = sd.mSwapchain->getExtent();
			mCallbackSwapchainResize(extent.width, extent.height, sd.mSwapchain);
		}
		else VK_CHECK(r, "failed to Queue Present!");
	}
	else
	{
		mDevice->getQueue(0, 0)->submitCommandBuffers({ currImageData.mCommandBuffer }, currImageData.mInFlightFences);
	}
}

void VulkanInstance::setupDebugCallback()
{
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugCreateInfo.pfnUserCallback = &debugCallback;

	// just for debugPrintfEXT
	VkDebugUtilsMessengerCreateInfoEXT debugPrintfCreateInfo = {};
	debugPrintfCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugPrintfCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT; // for GLSL_EXT_debug_printf
	debugPrintfCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
	debugPrintfCreateInfo.pfnUserCallback = &debugCallbackPrintf;
#ifndef NDEBUG
	VK_CHECK(mInstance->vk.CreateDebugUtilsMessengerEXT(mInstance->getHandle(), &debugCreateInfo, NULL, &mDebugCallback), "failed to set up debug callback!");
	VK_CHECK(mInstance->vk.CreateDebugUtilsMessengerEXT(mInstance->getHandle(), &debugPrintfCreateInfo, NULL, &mDebugCallbackPrintf), "failed to set up debug callback!");
#endif
}

void VulkanInstance::setDefaultState()
{

	// default render state for rpipeline
	RPipeline::global_render_state = {};
	RPipeline::global_render_state.dynamicStates.viewport = true;
	RPipeline::global_render_state.dynamicStates.scissor = true;

	Swapchain* sc = mSwapchainData.mSwapchain;
	if (sc) {
		const VkExtent2D extent = sc->getExtent();
		RPipeline::global_render_state.scissor.extent.width = sc ? extent.width : 100;
		RPipeline::global_render_state.scissor.extent.height = sc ? extent.height : 100;
		RPipeline::global_render_state.viewport.height = sc ? extent.height : 100;
		RPipeline::global_render_state.viewport.width = sc ? extent.width : 100;
		RPipeline::global_render_state.renderpass = sc->getRenderpassClear()->getHandle();
	}
	// color
	RPipeline::global_render_state.colorBlend[0].blendEnable = VK_TRUE;
	RPipeline::global_render_state.colorBlend[0].colorBlendOp = VK_BLEND_OP_ADD;
	RPipeline::global_render_state.colorBlend[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	RPipeline::global_render_state.colorBlend[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	RPipeline::global_render_state.colorBlend[0].alphaBlendOp = VK_BLEND_OP_ADD;
	RPipeline::global_render_state.colorBlend[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	RPipeline::global_render_state.colorBlend[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	RPipeline::global_render_state.colorBlend[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	// depth/stencil
	RPipeline::global_render_state.dsBlend.depthTestEnable = VK_TRUE;
	RPipeline::global_render_state.dsBlend.depthWriteEnable = VK_TRUE;
#ifdef RVK_USE_INVERSE_Z
	// Inverse Z: [1,0]
	RPipeline::global_render_state.dsBlend.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
	RPipeline::global_render_state.viewport.minDepth = 1;
	RPipeline::global_render_state.viewport.maxDepth = 0;
#else
	// Normal depth range: [0,1]
	RPipeline::global_render_state.dsBlend.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
#endif
}
	

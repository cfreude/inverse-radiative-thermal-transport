#include "imgui_vulkan.hpp"
#include <rvk/rvk.hpp>

#include <tamashii/engine/common/vars.hpp>
#include <tamashii/engine/platform/window.hpp>

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <sys/stat.h>

#if defined( _WIN32 )
#include <imgui_impl_win32.h>
#elif defined( __APPLE__ )
#include <imgui_impl_osx.h>
//#include "imgui_vulkan_osx.hpp"
#elif defined( __linux__ )
#if defined( VK_USE_PLATFORM_WAYLAND_KHR )

#elif defined( VK_USE_PLATFORM_XCB_KHR )
#include <tamashii/gui/imgui_impl_x11.h>
#endif
#endif

RVK_USE_NAMESPACE

namespace {
	void CheckVulkanResultCallback(const VkResult aResult)
	{
		if (aResult != VK_SUCCESS)
		{
			spdlog::error("ImGui Vulkan error ({})", rvk::error::errorString(aResult));
		}
	}
}

VkDescriptorPool ImGui_Vulkan_Impl::setup(rvk::CommandPool* aCommandPool, rvk::Queue* aQueue, rvk::Swapchain* aSc) {
	VkDescriptorPool pool = VK_NULL_HANDLE;
	constexpr VkDescriptorPoolSize descPoolSize = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 };
	const LogicalDevice* device = aSc->getDevice();
    if (pool != VK_NULL_HANDLE) device->vk.DestroyDescriptorPool(device->getHandle(), pool, VK_NULL_HANDLE);
    VkDescriptorPoolCreateInfo descPoolInfo = {};
	descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	descPoolInfo.maxSets = 1;
	descPoolInfo.poolSizeCount = 1;
	descPoolInfo.pPoolSizes = &descPoolSize;
	VK_CHECK(device->vk.CreateDescriptorPool(device->getHandle(), &descPoolInfo, NULL, &pool), " failed to create Descriptor Pool!");

	// Initialise ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	//ImPlot::CreateContext();

	// Initialise ImGui Vulkan adapter
	ImGui_ImplVulkan_InitInfo vulkanInit = {};
	vulkanInit.Instance = device->getPhysicalDevice()->getInstance()->getHandle();
	vulkanInit.PhysicalDevice = device->getPhysicalDevice()->getHandle();
	vulkanInit.Device = device->getHandle();
	vulkanInit.QueueFamily = aQueue->getFamily()->getIndex();
	vulkanInit.Queue = aQueue->getHandle();
	vulkanInit.PipelineCache = nullptr;
	vulkanInit.DescriptorPool = pool;
	vulkanInit.MinImageCount = 2;
	vulkanInit.ImageCount = aSc->getImageCount();
	vulkanInit.Allocator = nullptr;
	vulkanInit.CheckVkResultFn = CheckVulkanResultCallback;
	vulkanInit.ApiVersion = VK_API_VERSION_1_2;
	vulkanInit.PipelineInfoMain.RenderPass = aSc->getRenderpassClear()->getHandle();

	rvk::Instance* instance = device->getPhysicalDevice()->getInstance();
    ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_2, [](const char* aFunctionName, void*) { return InstanceManager::getInstance().vk.GetInstanceProcAddr(InstanceManager::getInstance().getInstances().front()->getHandle(), aFunctionName); });
    if (!ImGui_ImplVulkan_Init(&vulkanInit))
	{
		spdlog::error("failed to initialise ImGui vulkan adapter");
	}

	auto& io = ImGui::GetIO();
	// No ini file.
	io.IniFilename = nullptr;

	// Window scaling and style.
	//const auto scaleFactor = window.ContentScale();

	constexpr float scaleFactor = 1.1;
	ImGui::StyleColorsDark();
	ImGui::GetStyle().ScaleAllSizes(scaleFactor);

	// Upload ImGui fonts
    struct stat buffer;
	if (stat(tamashii::var::default_font.getString().c_str(), &buffer) == 0) {
		if (!io.Fonts->AddFontFromFileTTF(tamashii::var::default_font.getString().c_str(), 13 * scaleFactor))
		{
			spdlog::error("failed to load ImGui font");
		}
	}

	void* surfaceHandle = instance->getSurface(aSc->getSurfaceId()).mSurfaceHandles.first;
#ifdef WIN32
    ImGui_ImplWin32_Init(const_cast<void*>(surfaceHandle));
#elif defined( __APPLE__ )
    ImGui_ImplOSX_Init((NSView*)surfaceHandle);
#elif defined( __linux__ )
#if defined( VK_USE_PLATFORM_WAYLAND_KHR )

#elif defined( VK_USE_PLATFORM_XCB_KHR )
    ImGui_ImplX11_Init(*((xcb_window_t*)surfaceHandle));
#endif
#endif
	return pool;
}

void ImGui_Vulkan_Impl::destroy(const rvk::LogicalDevice* aDevice, VkDescriptorPool& aPool) {
	if (aPool != VK_NULL_HANDLE) {
		aDevice->vk.DestroyDescriptorPool(aDevice->getHandle(), aPool, VK_NULL_HANDLE);
		aPool = VK_NULL_HANDLE;
	}

	ImGui_ImplVulkan_Shutdown();
#ifdef WIN32
	ImGui_ImplWin32_Shutdown();
#elif defined( __APPLE__ )
    ImGui_ImplOSX_Shutdown();
#elif defined( __linux__ )
#if defined( VK_USE_PLATFORM_WAYLAND_KHR )

#elif defined( VK_USE_PLATFORM_XCB_KHR )
    ImGui_ImplX11_Shutdown();
#endif
#endif
	ImGui::DestroyContext();
	//ImPlot::DestroyContext();
}


void ImGui_Vulkan_Impl::prepare(const VkExtent2D aExtent) {
#ifdef WIN32
	ImGui_ImplWin32_NewFrame();
#elif defined( __APPLE__ )
    ImGui::GetIO().DisplaySize = ImVec2((float)aExtent.width, (float)aExtent.height);
    ImGui_ImplOSX_NewFrame(nullptr);
#elif defined( __linux__ )
#if defined( VK_USE_PLATFORM_WAYLAND_KHR )

#elif defined( VK_USE_PLATFORM_XCB_KHR )
    ImGui_ImplX11_NewFrame();
#endif
#endif
	ImGui_ImplVulkan_NewFrame();
	ImGui::NewFrame();
}


void ImGui_Vulkan_Impl::draw(const rvk::CommandBuffer* aCommandBuffer) {
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), aCommandBuffer->getHandle());
}

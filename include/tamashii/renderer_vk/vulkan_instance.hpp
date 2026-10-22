#pragma once
#include <tamashii/public.hpp>
#include <rvk/rvk.hpp>

T_BEGIN_NAMESPACE
class Window;
class VulkanInstance
{
public:
	VulkanInstance();
	~VulkanInstance() = default;

	void init(const std::vector<tamashii::Window*>& aWindows);
	void initImGui();
	void printDeviceInfos() const;
	void destroyImGui();
	void destroy();

	void setResizeCallback(const std::function<void(int, int, rvk::Swapchain*)>& aCallback);
	void recreateSwapchain(uint32_t aWidth, uint32_t aHeight);
	void beginFrame();
	void endFrame();

	enum class Vendor { UNDEFINED, NVIDIA, AMD, INTEL };
	Vendor								mVendor;
	std::vector<const char*>			mInstanceExtensions;
	std::vector<const char*>			mValidationLayers;
	std::vector<const char*>			mDeviceExtensions;

	void*								mLib;
	rvk::Instance*						mInstance;
	std::vector<std::string>			mSurfaces;
	rvk::LogicalDevice*					mDevice;

	// rescources
	std::thread::id						mMainThreadId;
	struct SwapchainImageData
	{
		rvk::Semaphore*					mNextImageAvailableSemaphores;
		rvk::Semaphore*					mRenderFinishedSemaphores;
		rvk::Fence*						mInFlightFences;
		rvk::CommandBuffer*				mCommandBuffer;
	};
	struct SwapchainData
	{
		tamashii::Window*					mWindow;
		std::string							mId;
		rvk::Swapchain*						mSwapchain;
		rvk::CommandPool*					mCommandPool;
		std::vector<SwapchainImageData>		mImages;
		uint32_t							mCurrentImageIndex;
		uint32_t							mPreviousImageIndex;
	};
	SwapchainData						mSwapchainData;
	std::vector<rvk::CommandPool*>		mCommandPools;

	std::function<void(int, int, rvk::Swapchain*)> mCallbackSwapchainResize;
	// debug
	VkDebugUtilsMessengerEXT			mDebugCallback;
	VkDebugUtilsMessengerEXT			mDebugCallbackPrintf;
	// imgui
	VkDescriptorPool					mImGuiPool;
private:
	void								setupDebugCallback();
	void								setDefaultState();
};
T_END_NAMESPACE

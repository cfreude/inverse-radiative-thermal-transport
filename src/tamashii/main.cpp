#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <tamashii/tamashii.hpp>
#include <tamashii/renderer_vk/render_backend.hpp>
#include <tamashii/implementations/rasterizer_default.hpp>
#include <tamashii/implementations/raytracing_default.hpp>

class VulkanRenderBackendDefault final : public tamashii::VulkanRenderBackend {
public:
	std::vector<tamashii::RenderBackendImplementation*> initImplementations(tamashii::VulkanInstance* aInstance) override
	{
		// current swapchain command buffer
		const std::function getCurrentCmdBuffer = [aInstance]()-> rvk::CommandBuffer*
		{
			return aInstance->mSwapchainData.mImages[aInstance->mSwapchainData.mCurrentImageIndex].mCommandBuffer;
		};
		// single time command buffer for two threads
		const std::function getStlCBuffer = [aInstance]()-> rvk::SingleTimeCommand
		{
			if (std::this_thread::get_id() == aInstance->mMainThreadId) return { aInstance->mCommandPools[0], aInstance->mDevice->getQueue(0, 1) };
			return { aInstance->mCommandPools[1], aInstance->mDevice->getQueue(0, 2) };
		};
		// current frame index
		const std::function getFrameIndex = [aInstance]()->uint32_t
		{
			return aInstance->mSwapchainData.mCurrentImageIndex;
		};
		return {
			new tamashii::rasterizerDefault(aInstance->mDevice, aInstance->mSwapchainData.mSwapchain, aInstance->mSwapchainData.mImages.size(), getFrameIndex, getCurrentCmdBuffer, getStlCBuffer),
			new tamashii::raytracingDefault(aInstance->mDevice, aInstance->mSwapchainData.mSwapchain, aInstance->mSwapchainData.mImages.size(), getFrameIndex, getCurrentCmdBuffer, getStlCBuffer)
		};
	}
};

//#include <tamashii/renderer_ogl/opengl_render_backend.hpp>
//class OpenGlRenderBackendDefault final : public tamashii::OpenGlRenderBackend {
//public:
//	std::vector<tamashii::RenderBackendImplementation*> initImplementations(OpenGlInstance* aInstance) override
//	{
//		return {};
//	}
//};

int main(int argc, char* argv[]) {
    tamashii::addBackend(new VulkanRenderBackendDefault);
	//tamashii::addBackend(new OpenGlRenderBackendDefault);
	tamashii::run(argc, argv);
	//tamashii::init(argc, argv);
	//tamashii::shutdown();
	
}

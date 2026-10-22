
#include <tamashii/tamashii.hpp>
#include <tamashii/engine/common/common.hpp>
#include <tamashii/engine/render/render_backend_implementation.hpp>
#include <tamashii/renderer_vk/render_backend.hpp>

class myImpl final : public tamashii::RenderBackendImplementation {
public:
					myImpl() = default;
					~myImpl() override = default;

	const char*		getName() override { return "myImpl"; }

	void			windowSizeChanged(int aWidth, int aHeight) override;

	// implementation preparation
	void			prepare(tamashii::renderInfo_s* aRenderInfo) override;
	void			destroy() override;

	// scene
	void			sceneLoad(tamashii::scene_s aScene) override;
	void			sceneUnload(tamashii::scene_s aScene) override;

	// frame
	void			drawView(tamashii::viewDef_s* aViewDef) override;
	void			drawUI(tamashii::uiConf_s* aUiConf) override;
};

void myImpl::windowSizeChanged(const int aWidth, const int aHeight) {}

// implementation preparation
void myImpl::prepare(tamashii::renderInfo_s* aRenderInfo) {}
void myImpl::destroy() {}
// scene load/unload
void myImpl::sceneLoad(tamashii::scene_s aScene) {}
void myImpl::sceneUnload(tamashii::scene_s aScene) {}
// draw frame
void myImpl::drawView(tamashii::viewDef_s* aViewDef) {}
void myImpl::drawUI(tamashii::uiConf_s* aUiConf) {}

#include <tamashii/implementations/rasterizer_default.hpp>
#include <tamashii/implementations/raytracing_default.hpp>
class VulkanRenderBackendEmpty final : public tamashii::VulkanRenderBackend {
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
			new tamashii::raytracingDefault(aInstance->mDevice, aInstance->mSwapchainData.mSwapchain, aInstance->mSwapchainData.mImages.size(), getFrameIndex, getCurrentCmdBuffer, getStlCBuffer),
			new myImpl()
		};
	}
};

int main(int argc, char* argv[]) {
	// use our vulkan backend
	tamashii::addBackend(new VulkanRenderBackendEmpty());
	// start
	tamashii::run(argc, argv);
}

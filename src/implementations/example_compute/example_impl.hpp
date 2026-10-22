#pragma once
#include <tamashii/engine/render/render_backend_implementation.hpp>
#include <tamashii/renderer_vk/render_backend.hpp>
#include <rvk/rvk.hpp>
class exampleComputeImpl final : public tamashii::RenderBackendImplementation {
public:
						exampleComputeImpl(rvk::LogicalDevice* aDevice, rvk::Swapchain* aSwapchain,
						                   std::function<rvk::CommandBuffer*()> aGetCurrentCmdBuffer,
						                   std::function<rvk::SingleTimeCommand()> aGetStCmdBuffer);
						~exampleComputeImpl() override = default;

	const char*			getName() override { return "Example Compute"; }
	void				windowSizeChanged(int aWidth, int aHeight) override;

						// implementation preparation
	void				prepare(tamashii::renderInfo_s* aRenderInfo) override;
	void				destroy() override;

						// scene
	void				sceneLoad(tamashii::scene_s aScene) override;
	void				sceneUnload(tamashii::scene_s aScene) override;

						// frame
    void				drawView(tamashii::viewDef_s* aViewDef) override;
    void				drawUI(tamashii::uiConf_s* aUiConf) override;

private:
	rvk::LogicalDevice* mDevice;
	rvk::Swapchain* mSwapchain;
	std::function<rvk::CommandBuffer*()> mGetCurrentCmdBuffer;
	std::function<rvk::SingleTimeCommand()> mGetStCmdBuffer;
};

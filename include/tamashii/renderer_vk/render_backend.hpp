#pragma once
#include <tamashii/engine/scene/render_scene.hpp>
#include <tamashii/engine/render/render_backend.hpp>
#include <tamashii/gui/imgui_gui.hpp>
#include "vulkan_instance.hpp"
#include <atomic>

T_BEGIN_NAMESPACE
class VulkanRenderBackend : public RenderBackend {
public:
										VulkanRenderBackend();
										~VulkanRenderBackend() override;

	const char*							getName() override { return "vulkan"; }
	virtual std::vector<RenderBackendImplementation*> initImplementations(VulkanInstance* aInstance) { return {}; }

										// setup
										// create and manage context
    void								init(Window* aMainWindow) override;
	void								shutdown() override;

	void								addImplementation(RenderBackendImplementation* aImplementation) override;

	void								reloadImplementation(scene_s aScene) override;
	void								changeImplementation(uint32_t aIndex, scene_s aScene) override;
	std::vector<RenderBackendImplementation*>& getAvailableBackendImplementations() override;
	RenderBackendImplementation*		getCurrentBackendImplementations() const override;
										// call when render surface size changes
	void								recreateRenderSurface(uint32_t aWidth, uint32_t aHeight) override;
	void								entitiyAdded(const Ref_s* aRef) const override;
	void								entitiyRemoved(const Ref_s* aRef) const override;
	bool								drawOnMesh(const DrawInfo_s* aDrawInfo) const override;
	void								screenshot(const std::string& aName) const override;

										// scene
										// load and unload everything related to the current scene
	void								sceneLoad(scene_s aScene) override;
	void								sceneUnload(scene_s aScene) override;

										// frame
	void								beginFrame() override;
    void								drawView(viewDef_s* aViewDef) override;
    void								drawUI(uiConf_s* aUiConf) override;
    void								captureSwapchain(screenshotInfo_s* aScreenshotInfo) override;
	void								endFrame() override;
private:
										// prepare/destroy current implementation
	void								prepare() const override;
	void								destroy() const override;

	VulkanInstance						mInstance;

    std::atomic<bool>                   mSceneLoaded;
    std::atomic<bool>                   mCmdRecording;

    int									mCurrentImplementationIndex = -1;
    std::vector<RenderBackendImplementation*>mImplementations;

	bool								mActiveGui;
	MainGUI								mMainGui;
};

T_END_NAMESPACE

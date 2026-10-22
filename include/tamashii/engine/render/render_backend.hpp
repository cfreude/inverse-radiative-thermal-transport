#pragma once
#include <tamashii/public.hpp>
#include <tamashii/engine/scene/render_scene.hpp>

T_BEGIN_NAMESPACE
struct viewDef_s;
struct Ref_s;
struct uiConf_s;
struct screenshotInfo_s;
struct DrawInfo_s;
struct HitInfo_s;
class Window;
class RenderBackendImplementation;
class RenderBackend {
public:
										RenderBackend() = default;
	virtual								~RenderBackend() = default;
	virtual const char*					getName() = 0;
										// setup/destroy context
										// Window ->getWindowHandle(), ->getInstanceHandle()
	virtual void						init(Window* aMainWindow) = 0;
	virtual void						shutdown() = 0;

	virtual void						addImplementation(RenderBackendImplementation* aImplementation) = 0;
	virtual void						reloadImplementation(scene_s aScene) = 0;
	virtual void						changeImplementation(uint32_t aIndex, scene_s aScene) = 0;
	virtual RenderBackendImplementation* getCurrentBackendImplementations() const = 0;
	virtual std::vector<RenderBackendImplementation*>& getAvailableBackendImplementations() = 0;

										// various callback
	virtual void						recreateRenderSurface(const uint32_t aWidth, const uint32_t aHeight) {}
	virtual void						entitiyAdded(const Ref_s* aRef) const {}
	virtual void						entitiyRemoved(const Ref_s* aRef) const {}
	virtual bool						drawOnMesh(const DrawInfo_s* aDrawInfo) const { return false; }
	virtual void						screenshot(const std::string& aName) const {}

										// scene
										// load and unload everything related to the current scene
	virtual void						sceneLoad(scene_s aScene) = 0;
	virtual void						sceneUnload(scene_s aScene) = 0;

										// frame
	virtual void						beginFrame() = 0;
	virtual void						drawView(viewDef_s* aViewDef) = 0;
	virtual void						drawUI(uiConf_s* aUiConf) = 0;
	virtual void						captureSwapchain(screenshotInfo_s* aScreenshotInfo) {}
	virtual void						endFrame() = 0;
private:
	// prepare/destroy current implementation
	virtual void						prepare() const = 0;
	virtual void						destroy() const = 0;
};
T_END_NAMESPACE
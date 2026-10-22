#pragma once
#include <tamashii/public.hpp>
#include <tamashii/engine/scene/material.hpp>
#include <tamashii/engine/scene/image.hpp>
#include <tamashii/engine/scene/camera.hpp>
#include <tamashii/engine/scene/model.hpp>
#include <tamashii/engine/scene/light.hpp>
#include <tamashii/engine/scene/render_cmd_system.hpp>

T_BEGIN_NAMESPACE
class RenderBackendImplementation {
public:
					RenderBackendImplementation() = default;
	virtual			~RenderBackendImplementation() = default;

	virtual const char*	getName() = 0;

	virtual void	windowSizeChanged(const int aWidth, const int aHeight) {}
	virtual void	entityAdded(const Ref_s* aRef) {}
	virtual void	entityRemoved(const Ref_s* aRef) {}
					// gets called when draw mode is activated, return false if no drawing happened
					// use InputSystem::getInstance().isDown(Input::MOUSE_LEFT) to get input
	virtual bool	drawOnMesh(const DrawInfo_s* aDrawInfo) { return false; }
	virtual void	screenshot(const std::string& aFilename) { spdlog::warn("Impl-Screenshot function not implemented"); }

					// implementation preparation
	virtual void	prepare(renderInfo_s* aRenderInfo) = 0;
	virtual void	destroy() = 0;

					// scene
	virtual void	sceneLoad(scene_s aScene) = 0;
	virtual void	sceneUnload(scene_s aScene) = 0;

					// frame
	virtual void	drawView(viewDef_s* aViewDef) = 0;
	virtual void	drawUI(uiConf_s* aUiConf) = 0;
};
T_END_NAMESPACE
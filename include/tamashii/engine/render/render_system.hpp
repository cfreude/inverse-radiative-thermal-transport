#pragma once
#include <tamashii/public.hpp>
#include <tamashii/engine/scene/render_scene.hpp>

#include <list>

T_BEGIN_NAMESPACE
class RenderBackend;
class RenderScene;
class RenderBackendImplementation;
struct RenderConfig_s {
	// statistic
	uint32_t			frame_index;
	// smoothed over multiple frames
	float				framerate_smooth;
	float				frametime_smooth;
	// raw time
	float				frametime; /* in milliseconds */

	bool				show_gui;
	bool				mark_lights;
};

class RenderSystem {
public:
										RenderSystem();
										~RenderSystem();

	void								addBackend(RenderBackend* aRenderBackend);

	void								init();
	void								shutdown();

	RenderConfig_s&						getConfig();

	void								renderSurfaceResize(uint32_t aWidth, uint32_t aHeight) const;
	bool								drawOnMesh(const DrawInfo_s* aDrawInfo) const;

	void								processCommands();
	// scene
	RenderScene*						getMainScene() const;
	RenderScene*						allocRenderScene();
	void								freeRenderScene(RenderScene* aRenderScene);
	void								setMainRenderScene(RenderScene* aRenderScene);
	// (un)load current main scene
	void								sceneLoad(scene_s aScene) const;
	void								sceneUnload(scene_s aScene) const;
	void								reloadCurrentScene() const;

	// swap between backend implementations
	void								addImplementation(RenderBackendImplementation* aImplementation) const;
	void								reloadBackendImplementation() const;
	void								changeBackendImplementation(int aIndex) const;
	std::vector<RenderBackendImplementation*>& getAvailableBackendImplementations() const;
	RenderBackendImplementation*		getCurrentBackendImplementations() const;
private:

	void								updateStatistics();

	RenderConfig_s						mRenderConfig;

	// scenes associated with this system
	std::list<RenderScene*>				mScenes;
	RenderScene*						mMainScene;

	// the renderer of this render system
	std::vector<RenderBackend*>			mRenderBackends;
	RenderBackend*						mCurrendRenderBackend;
};
T_END_NAMESPACE
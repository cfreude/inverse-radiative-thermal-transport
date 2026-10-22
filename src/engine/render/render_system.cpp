#include <tamashii/engine/render/render_system.hpp>
#include <tamashii/engine/render/render_backend.hpp>
#include <tamashii/engine/render/render_backend_implementation.hpp>
#include <tamashii/engine/common/common.hpp>
#include <tamashii/engine/common/vars.hpp>
#include <tamashii/engine/exporter/exporter.hpp>

#include <array>
#include <chrono>

T_USE_NAMESPACE

RenderSystem::RenderSystem() : mRenderConfig(), mMainScene(nullptr), mCurrendRenderBackend(nullptr)
{
	mRenderBackends.reserve(3);
}

RenderSystem::~RenderSystem()
= default;

RenderConfig_s& RenderSystem::getConfig()
{ return mRenderConfig; }

bool RenderSystem::drawOnMesh(const DrawInfo_s* aDrawInfo) const
{ return mCurrendRenderBackend->drawOnMesh(aDrawInfo); }

RenderScene* RenderSystem::getMainScene() const
{ return mMainScene; }

void RenderSystem::setMainRenderScene(RenderScene* aRenderScene)
{ mMainScene = aRenderScene; }

void RenderSystem::sceneLoad(const scene_s aScene) const
{ if(mCurrendRenderBackend) mCurrendRenderBackend->sceneLoad(aScene); }

void RenderSystem::sceneUnload(const scene_s aScene) const
{ if(mCurrendRenderBackend) mCurrendRenderBackend->sceneUnload(aScene); }

void RenderSystem::addImplementation(RenderBackendImplementation* aImplementation) const
{ if(mCurrendRenderBackend) mCurrendRenderBackend->addImplementation(aImplementation); }

void RenderSystem::reloadBackendImplementation() const
{
	const scene_s s = mMainScene->getSceneData();
	mCurrendRenderBackend->reloadImplementation(s);
}

std::vector<RenderBackendImplementation*>& RenderSystem::getAvailableBackendImplementations() const
{ return mCurrendRenderBackend->getAvailableBackendImplementations(); }

RenderBackendImplementation* RenderSystem::getCurrentBackendImplementations() const
{ return mCurrendRenderBackend->getCurrentBackendImplementations(); }

void RenderSystem::addBackend(RenderBackend* aRenderBackend)
{
	mRenderBackends.push_back(aRenderBackend);
}

void RenderSystem::init()
{
	mRenderConfig.show_gui = true;
	mRenderConfig.mark_lights = true;
	mRenderConfig.frame_index = 0;

	mMainScene = allocRenderScene();
	mMainScene->readyToRender(true);

	if(mRenderBackends.empty())
	{
		spdlog::error("No render backend set!");
		Common::getInstance().shutdown();
		return;
	}
	mCurrendRenderBackend = mRenderBackends.front();
	for(RenderBackend* rb : mRenderBackends) {
		if (var::render_backend.getString() == rb->getName()) {
			mCurrendRenderBackend = rb;
			break;
		}
	}
	mCurrendRenderBackend->init(Common::getInstance().getMainWindow()->window());
}

void RenderSystem::shutdown()
{
	for (RenderScene* rs : mScenes) {
		sceneUnload(rs->getSceneData());
		delete rs;
	}
	mScenes.clear();
	mMainScene = nullptr;
	//if (mCurrendRenderBackend) mCurrendRenderBackend->shutdown(); // caused some mem-access error in single-thread mode (ToDo: find out why and fix properly)
	for(const RenderBackend* rb : mRenderBackends) delete rb;
	mRenderBackends.clear();
}

void RenderSystem::renderSurfaceResize(const uint32_t aWidth, const uint32_t aHeight) const
{
	mCurrendRenderBackend->recreateRenderSurface(aWidth, aHeight);
}

void RenderSystem::processCommands()
{
	RCmd_s cmd{ RenderCommand::EMPTY, {nullptr} };
	while(renderCmdSystem.nextCmd()) {
		cmd = renderCmdSystem.popNextCmd();
		switch (cmd.mType) {
			case RenderCommand::BEGIN_FRAME:
				updateStatistics();
				mCurrendRenderBackend->beginFrame();
				break;
			case RenderCommand::DRAW_VIEW:
				cmd.mViewDef->frame_index = mRenderConfig.frame_index;
				mCurrendRenderBackend->drawView(cmd.mViewDef);
				break;
			case RenderCommand::DRAW_UI:
				mCurrendRenderBackend->drawUI(cmd.mUiConf);
				break;
			case RenderCommand::ENTITY_ADDED:
				mCurrendRenderBackend->entitiyAdded(cmd.mEntity);
				break;
			case RenderCommand::ENTITY_REMOVED:
				mCurrendRenderBackend->entitiyRemoved(cmd.mEntity);
				break;
			case RenderCommand::SCREENSHOT:
				mCurrendRenderBackend->captureSwapchain(cmd.mScreenshotInfo);
				Exporter::save_image_png_8_bit(cmd.mScreenshotInfo->name, cmd.mScreenshotInfo->width, cmd.mScreenshotInfo->height, cmd.mScreenshotInfo->channels, cmd.mScreenshotInfo->data);
				break;
			case RenderCommand::IMPL_SCREENSHOT:
			{
				std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
				mCurrendRenderBackend->screenshot(std::to_string(now.time_since_epoch().count()));
				break;
			}
			case RenderCommand::IMPL_DRAW_ON_MESH:
			{
				bool result = mCurrendRenderBackend->drawOnMesh(cmd.mDrawInfo);
				break;
			}
			case RenderCommand::END_FRAME:
			{
				std::optional<std::lock_guard<std::mutex>> optionalLock;
				if(Common::getInstance().getMainWindow()) optionalLock.emplace(Common::getInstance().getMainWindow()->window()->resizeMutex());
				mCurrendRenderBackend->endFrame();
				optionalLock.reset();
				break;
			}
			case RenderCommand::ASSET_ADDED:
			case RenderCommand::ASSET_REMOVED:
			case RenderCommand::EMPTY: break;
		}
		renderCmdSystem.deleteCmd(cmd);
	}
}

RenderScene* RenderSystem::allocRenderScene()
{
	const auto rs = new RenderScene();
	mScenes.push_back(rs);
	return rs;
}

void RenderSystem::freeRenderScene(RenderScene* aRenderScene)
{
	if (!aRenderScene) return;
	mScenes.remove(aRenderScene);
	delete aRenderScene;
	aRenderScene = nullptr;
}

void RenderSystem::reloadCurrentScene() const
{
	const scene_s s = mMainScene->getSceneData();
	mMainScene->readyToRender(false);
	sceneUnload(s);
	sceneLoad(s);
	mMainScene->readyToRender(true);
}

void RenderSystem::changeBackendImplementation(const int aIndex) const
{
	const scene_s s = mMainScene->getSceneData();
	mCurrendRenderBackend->changeImplementation(aIndex, s);
}

void RenderSystem::updateStatistics()
{
	mRenderConfig.frame_index++;

	// fps and frametime
	{
		static std::array<float, FPS_FRAMES> previousTimes;
		static int index;
		static std::chrono::time_point<std::chrono::high_resolution_clock> previous;

		std::chrono::time_point<std::chrono::high_resolution_clock> t = std::chrono::high_resolution_clock::now();

		// fps limiter
		if (var::max_fps.getInt()) {
			const float target = (1.0f / static_cast<float>(var::max_fps.getInt())) * 1000.0f;
			while (std::chrono::duration<float, std::milli>(t - previous).count() < target) {
				t = std::chrono::high_resolution_clock::now();
			}
		}

		mRenderConfig.frametime = std::chrono::duration<float, std::milli>(t - previous).count();
		previous = t;

		mRenderConfig.frametime_smooth = 0;

		previousTimes[index % FPS_FRAMES] = mRenderConfig.frametime;
		index++;
		if (index > FPS_FRAMES)
		{
			// average multiple frames together to smooth changes out a bit
			mRenderConfig.frametime_smooth = 0;
			for (int i = 0; i < FPS_FRAMES; i++)
			{
				mRenderConfig.frametime_smooth += previousTimes[i];
			}
			mRenderConfig.frametime_smooth = mRenderConfig.frametime_smooth / FPS_FRAMES;
			mRenderConfig.framerate_smooth = 1000.0f / mRenderConfig.frametime_smooth;
		}
	}
}


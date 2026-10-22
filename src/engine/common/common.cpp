#include <tamashii/engine/common/common.hpp>
#include <tamashii/engine/common/input.hpp>
#include <tamashii/engine/common/var_system.hpp>
#include <tamashii/engine/common/vars.hpp>
#include <tamashii/engine/scene/camera.hpp>
#include <tamashii/engine/scene/model.hpp>
#include <tamashii/engine/scene/render_cmd_system.hpp>
#include <tamashii/engine/render/render_system.hpp>
#include <tamashii/engine/importer/importer.hpp>
#include <tamashii/engine/exporter/exporter.hpp>
#include <tamashii/engine/topology/topology.hpp>
#include <tamashii/engine/platform/system.hpp>
#include <tamashii/engine/platform/filewatcher.hpp>

#include <glm/gtc/color_space.hpp>
#include <chrono>
#include <filesystem>

T_USE_NAMESPACE
std::atomic_bool Common::mFileDialogRunning = false;
void Common::init(const int aArgc, char* aArgv[], char* aCmdline)
{
	spdlog::set_pattern("%H:%M:%S.%e %^%l%$ %v");
#ifndef NDEBUG
	spdlog::set_level(spdlog::level::debug);
#endif
	// get system infos etc
	sys::initSystem(mSystemInfo);
	spdlog::info("Starting " + var::program_name.getString());

	// process cl arguments
	mCmdArgs.clear();
	if (aArgc) mCmdArgs.setArgs(aArgc, aArgv);
	else if (aCmdline) mCmdArgs.setArgs(aCmdline);
	parseCommandLine(mCmdArgs);
	// init command system
	mCmdSystem.init();
	// init var system
	mVarSystem.init();
	// add all vars to the varsystem that were created befor the init of varSystem
	Var::registerStaticVars();
	// read config file (command line arguments can overwrite settings from config file)
	loadConfig();
	// process command line input (set variables and add commands)
	setStartupVariables();
	addStartupCommands();
	
	std::filesystem::current_path(var::work_dir.getValue());
	spdlog::info("Working Dir: '{}'", std::filesystem::current_path().string());
	std::filesystem::create_directories(var::cache_dir.getString() + "/shader");
	spdlog::info("Cache Dir: '{}'", std::filesystem::path(std::filesystem::current_path().string() + "/" + var::cache_dir.getString()).make_preferred().string());
	spdlog::info("Config: '{}'", std::filesystem::path(std::filesystem::current_path().string() + "/" + var::cfg_filename.getString()).make_preferred().string());

	spdlog::info("Initializing renderer");
	// setup rendering (window and render system)
	mRenderSystem.init();

	// file watcher
	FileWatcher& watcher = FileWatcher::getInstance();
	mFileWatcherThread = watcher.spawn();

	// execute all the commands added from addStartupCommands
	mCmdSystem.execute();
	EventSystem::getInstance().eventLoop();
}

void Common::shutdown()
{
	mShutdown = true;
	if (!mFrameThread.joinable()) shutdownIntern();
}

bool Common::frame()
{
	// if window and frame thread should be separate => spawn
	if (mWindow.isWindow() && var::window_thread.getBool() && !mFrameThread.joinable()) {
		spdlog::info("Using separate window thread");
		mFrameThread = std::thread([&] { while (frameIntern()) {} });
	}

	// window events
	mWindow.frame();
	// if no dedicated thread, execute in current thread
	if(!mFrameThread.joinable()) frameIntern();

	if (mShutdown) shutdownIntern();
	return !mShutdown;
}

Common::Common() : mSystemInfo({}), mShutdown(false), mIntersectionSettings(CullMode_e::None, HitMask_e::All),
                   mDrawInfo({}), mScreenshotNoUi(false)
{
}

void Common::shutdownIntern()
{
	spdlog::info("Shutting down " + var::program_name.getString());

	writeConfig();
	// threads
	FileWatcher::getInstance().terminate();
	if (mFileWatcherThread.joinable()) mFileWatcherThread.join();
	if (mSceneLoaderThread.joinable()) mSceneLoaderThread.join();
	if (mFrameThread.joinable()) mFrameThread.join();
	if (mRenderThread.joinable()) mRenderThread.join();

	mRenderSystem.shutdown();
	if (mWindow.isWindow()) mWindow.shutdown();
}

bool Common::frameIntern()
{
	// process messages send to the window (mouse/key input, resize operation, etc)
	EventSystem::getInstance().eventLoop();
	if (mShutdown) return false;

	// stop rendering when window is minimized
	if (mWindow.isMinimized()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		return true;
	}

	processInputs();
	while (renderCmdSystem.frames() >= 1) {}// std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>((1.0f / 144.0f) * 1000.0f)));

	// record commands
	renderCmdSystem.addBeginFrameCmd();
	RenderScene* main_scene = mRenderSystem.getMainScene();
	if (main_scene->getCurrentCamera()->mode == RefCamera_s::Mode::EDITOR && mDrawInfo.mDrawMode && mDrawInfo.mHitInfo.mHit && mDrawInfo.mTarget == DrawInfo_s::Target_e::CUSTOM) renderCmdSystem.addDrawOnMeshCmd(&mDrawInfo, &mDrawInfo.mHitInfo);
	if (main_scene->animation()) main_scene->update(mRenderSystem.getConfig().frametime);
	main_scene->draw(); // prepare render command

	if (!mScreenshotNoUi) {
		const auto uc = new uiConf_s;
		// scene
		uc->scene = main_scene;
		// statistic
		uc->system = &mRenderSystem;
		// draw
		uc->draw_info = &mDrawInfo;
		renderCmdSystem.addDrawUICmd(uc);
	}
	else mScreenshotNoUi = false;
	renderCmdSystem.addEndFrameCmd();

	if (!mScreenshotPath.empty())
	{
		if (!mScreenshotPath.has_extension()) mScreenshotPath = mScreenshotPath.string() + ".png";
		renderCmdSystem.addScreenshotCmd(mScreenshotPath.string());
		spdlog::info("Screenshot saved: {}", mScreenshotPath.string());
		mScreenshotPath.clear();
	}

	// process commands
	if (!var::render_thread.getBool()) mRenderSystem.processCommands();
	else if (!mRenderThread.joinable()) {
		spdlog::warn("Using separate frame/render thread -> very experimental");
		mRenderThread = std::thread([&] { while (!mShutdown) mRenderSystem.processCommands(); });
	}
	
	return true;
}

void Common::parseCommandLine(CmdArgs const& aArgs)
{
	CmdArgs clArg;
	for (std::string s : aArgs) {
		if (s.at(0) == '-')
		{
			// add to arg list
			if (clArg.argc() != 0) mCommandLineArgs.push_back(clArg);
			clArg = CmdArgs();

			// handle -var=value
			size_t pos = 0;
			if ((pos = s.find('=')) != std::string::npos) {
				clArg.appendArg(s.substr(1, pos - 1));
				clArg.appendArg(s.substr(pos + 1, s.size()));
			}
			// handle -var value
			else clArg.appendArg(s.substr(1, s.size()));
		}
		else {
			clArg.appendArg(s);
		}
	}
	// add to arg list
	if (clArg.argc() != 0) mCommandLineArgs.push_back(clArg);
}

void Common::setStartupVariables(const std::string& aMatch)
{	
	// only execute set commands right now
	for (CmdArgs &arg : mCommandLineArgs) {
		const Var* var = mVarSystem.find(arg.argv(0));
		if (var == nullptr) {
			spdlog::warn("var '{}' not found", arg.argv(0));
			continue;
		}
		const std::string cmd = var->getCmd();
		if (!cmd.compare("set_var")) {
			mCmdSystem.cmdArg(CmdSystem::Execute::NOW, arg);
		}
	}

    if(glm::any(glm::equal(var::window_size.getInt2(), glm::ivec2(0)))) var::window_size.setInt2({1280, 720});
	mDrawInfo.mCursorColor = glm::vec3(1);
	mDrawInfo.mRadius = 0.1f;
	mDrawInfo.mColor0 = glm::vec4(1, 1, 1, 1);
	mDrawInfo.mDrawRgb = true;
	mDrawInfo.mDrawAlpha = true;
}

void Common::addStartupCommands()
{
	for (CmdArgs& arg : mCommandLineArgs) {
		const Var* var = mVarSystem.find(arg.argv(0));
		if (var == nullptr) continue;
		const std::string cmd = var->getCmd();
		if (cmd != "set_var") mCmdSystem.cmdArg(CmdSystem::Execute::APPEND, arg);
	}
}

void Common::loadConfig()
{
	mCfg.load(var::cfg_filename.getString());

	for (const auto& entry : mCfg) {
		// update tVars if var is marked as read
		Var* var = mVarSystem.find(entry.first);
		if (var && (var->getFlags() & Var::CONFIG_RD)) {
			// set var to the value from the cfg file
			var->setValue(entry.second);
		}
	}
}

void Common::writeConfig()
{
    if(!var::headless.getBool()){
		if (!mWindow.isMaximized()) {
			var::window_pos.setInt2(mWindow.getPosition());
			var::window_size.setInt2(mWindow.getSize());
		}
		var::window_maximized.setBool(mWindow.isMaximized());
    }
	// check all registered vars
	for (auto const& var : mVarSystem)
	{
		// check for changes between engine vars and values from config file
		if (var.second->getFlags() & Var::CONFIG_RDWR) {
			// check if var is in the loaded config
			if (mCfg.inside(var.first)) {
				// if var value is different than loaded value -> update cfg file
				if (var.second->getValue() != mCfg.get(var.first)) {
					mCfg.update(var.first, var.second->getValue());
				}
			}
			// if var is not present in loaded cfg but is read/write, add it to the config with the current value
			else if ((var.second->getFlags() & Var::CONFIG_RDWR) == Var::CONFIG_RDWR) {
				mCfg.add(var.first, var.second->getValue());
			}
		}
	}
	if (mCfg.wasUpdated()) {
		const std::string out = var::cfg_filename.getString();
		spdlog::info("...writing config: '{}'", out);
		mCfg.write(out);
	}
}

void Common::processInputs()
{
	const InputSystem& is = InputSystem::getInstance();
	// open file dialogs
	if (is.isDown(Input::KEY_LCTRL))
	{
		mWindow.ungrabMouse();// free mouse when in fps mode
		if (is.wasPressed(Input::KEY_O)) openFileDialogOpenScene();
		else if (is.wasPressed(Input::KEY_A)) openFileDialogAddScene();
		else if (is.wasPressed(Input::KEY_M)) openFileDialogOpenModel();
		else if (is.wasPressed(Input::KEY_L)) openFileDialogOpenLight();
	}

	RenderScene* mainScene = mRenderSystem.getMainScene();
	if (!mainScene) return;
	if (!mainScene->getCurrentCamera()) return;
	// capture mouse/keyboard when in fps camera mode
	if (mainScene->getCurrentCamera()->mode == RefCamera_s::Mode::FPS) {
		if (is.wasReleased(Input::MOUSE_LEFT)) mWindow.grabMouse();
		if (is.wasPressed(Input::KEY_ESCAPE)) mWindow.ungrabMouse();
	}
	// select/draw when in editor mode
	if (mainScene->getCurrentCamera()->mode == RefCamera_s::Mode::EDITOR) {
		// reset selection/draw
		if (is.wasPressed(Input::KEY_ESCAPE)) {
			mainScene->setSelection(nullptr);
			mDrawInfo.mDrawMode = mDrawInfo.mHoverOver = false;
		}

		if (mDrawInfo.mDrawMode) {
			if (is.isDown(Input::KEY_LCTRL) && is.getMouseWheelRelative().y != 0.0f) {
				mDrawInfo.mRadius += 0.01f * is.getMouseWheelRelative().y;
			}

			HitInfo_s& hi = mDrawInfo.mHitInfo;
			hi = {};
			intersectScene({ mIntersectionSettings.mCullMode, HitMask_e::Geometry }, &hi);
			if (hi.mHit) {
				const triangle_s tri = hi.mRefMeshHit->mesh->getTriangle(hi.mPrimitiveIndex, &hi.mHit->model_matrix);
				mDrawInfo.mHoverOver = true;
				mDrawInfo.mPositionWs = hi.mHitPos;
				mDrawInfo.mNormalWsNorm = tri.mGeoN;
				mDrawInfo.mTangentWsNorm = topology::calcStarkTangent(mDrawInfo.mNormalWsNorm);

				if (mDrawInfo.mTarget != DrawInfo_s::Target_e::CUSTOM) paintOnMesh(&hi);
			}
			else mDrawInfo.mHoverOver = false;
		}
		// normal mode
		else {
			if (is.wasReleased(Input::KEY_D)) {
				mDrawInfo.mDrawMode = true;
				mainScene->setSelection(nullptr);
			}
			if (is.wasReleased(Input::MOUSE_LEFT)) {
				HitInfo_s hi = {};
				intersectScene(mIntersectionSettings, &hi);
				mainScene->setSelection(hi.mHit);
			}
			Ref_s* s = mainScene->getSelection();
			if (is.wasPressed(Input::KEY_X) && s) {
				if (s->type == Ref_s::Type::Light) {
					mainScene->removeLight(reinterpret_cast<RefLight_s*>(s));
				}
				else if (s->type == Ref_s::Type::Model) {
					mainScene->removeModel(reinterpret_cast<RefModel_s*>(s));
				}
				mainScene->setSelection(nullptr);
			}
		}
	}
	else mainScene->setSelection(nullptr);

	RenderConfig_s& rc = mRenderSystem.getConfig();
	if (is.wasPressed(Input::KEY_F1)) rc.show_gui = !rc.show_gui;
	if (is.wasPressed(Input::KEY_F2)) rc.mark_lights = !rc.mark_lights;
	if (is.wasPressed(Input::KEY_F5)) {
		const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
		const std::string name = std::filesystem::path(std::filesystem::current_path().string() + "/" + SCREENSHOT_FILE_NAME + "_" + std::to_string(now.time_since_epoch().count())).make_preferred().string();
		mScreenshotPath = name + ".png";
	}
	if (is.wasPressed(Input::KEY_F6)) renderCmdSystem.addImplScreenshotCmd();
	
	// update cams
	RefCamera_s* refCam = mainScene->getCurrentCamera();
	if (!refCam) return;
	const glm::ivec2 surfaceSize = var::headless.getBool() ? var::render_size.getInt2() : mWindow.getSize();
	bool camUpdated = false;
	// update projection matrix
	Camera* cam = refCam->camera;
	if (cam->getType() == Camera::Type::PERSPECTIVE) camUpdated |= cam->updateAspectRatio(surfaceSize);
	else if (cam->getType() == Camera::Type::ORTHOGRAPHIC) camUpdated |= cam->updateMag(surfaceSize);
	// update view matrix
	const auto refCamPrivate = static_cast<RefCameraPrivate_s*>(refCam);
	const glm::vec2 mousePosRelative = is.getMousePosRelative();
	const glm::vec2 mouseWheelRelative = is.getMouseWheelRelative();
	if(refCam->mode == RefCamera_s::Mode::FPS && mWindow.isMouseGrabbed())
	{
		camUpdated |= refCamPrivate->updatePosition(is.isDown(Input::KEY_W), is.isDown(Input::KEY_S),
			is.isDown(Input::KEY_A), is.isDown(Input::KEY_D), is.isDown(Input::KEY_E), is.isDown(Input::KEY_Q));
		camUpdated |= refCamPrivate->updateAngles(mousePosRelative.x, mousePosRelative.y);
		camUpdated |= refCamPrivate->updateSpeed(mouseWheelRelative.y);
	} else if (refCam->mode == RefCamera_s::Mode::EDITOR)
	{
		if (is.isDown(Input::KEY_LSHIFT) && is.isDown(Input::MOUSE_WHEEL)) camUpdated |= refCamPrivate->updateCenter(mousePosRelative);
		else if (is.isDown(Input::MOUSE_WHEEL)) camUpdated |= refCamPrivate->drag(glm::vec2(mousePosRelative));
		else if (!is.isDown(Input::KEY_LCTRL)) camUpdated |= refCamPrivate->updateZoom(mouseWheelRelative.y);
	}
	if (camUpdated) {
		refCamPrivate->updateMatrix(true);
		mainScene->requestCameraUpdate();
	}
}

void Common::paintOnMesh(const HitInfo_s *aHitInfo) const
{
	if (mDrawInfo.mTarget == DrawInfo_s::Target_e::VERTEX_COLOR) {
		auto color = glm::vec4(0.0f);
		if (InputSystem::getInstance().isDown(Input::MOUSE_LEFT)) color = mDrawInfo.mColor0;
		else if (InputSystem::getInstance().isDown(Input::MOUSE_RIGHT)) color = mDrawInfo.mColor1;
		else return;

		// remove gamma
		color = glm::vec4(glm::convertSRGBToLinear(glm::vec3(color)), color.w);

		const glm::vec3 hitN = glm::normalize(aHitInfo->mRefMeshHit->mesh->getTriangle(aHitInfo->mPrimitiveIndex).mGeoN);
		for (Mesh* mesh : *reinterpret_cast<RefModel_s*>(aHitInfo->mHit)->model) {
			std::vector<vertex_s>* vertices = mesh->getVerticesVector();
			for (vertex_s& v : *vertices) {
				auto newColor = glm::vec4(0);
				const float ndotn = glm::dot(hitN, glm::vec3(v.normal));
				if (mDrawInfo.mDrawAll) {}
				else {
					// TODO: could be improve with geo normal
					if (ndotn < 0) continue;
					const float dist = glm::length(glm::vec3(((RefModel_s*)aHitInfo->mHit)->model_matrix * v.position) - mDrawInfo.mPositionWs);
					if (dist <= mDrawInfo.mRadius) {
						if (mDrawInfo.mSoftBrush) {
							const float ratio = dist / mDrawInfo.mRadius;
							glm::mix(v.color_0, color, ratio);
							if (InputSystem::getInstance().isDown(Input::MOUSE_LEFT)) newColor = glm::mix(color, v.color_0, ratio * ndotn);
							else if (InputSystem::getInstance().isDown(Input::MOUSE_RIGHT)) newColor = glm::mix(color, v.color_0, ratio * ndotn);
						}
					}
					else continue;
				}

				if (mDrawInfo.mDrawRgb) {
					v.color_0.x = mDrawInfo.mSoftBrush ? newColor.x : color.x;
					v.color_0.y = mDrawInfo.mSoftBrush ? newColor.y : color.y;
					v.color_0.z = mDrawInfo.mSoftBrush ? newColor.z : color.z;
				}
				if (mDrawInfo.mDrawAlpha) v.color_0.w = mDrawInfo.mSoftBrush ? newColor.w : color.w;
			}
		}
	}
	else return;
	RenderScene *mainScene = mRenderSystem.getMainScene();
	mainScene->requestModelGeometryUpdate();
}

void Common::newScene()
{
	// wait for previous scene loader thread
	if (mSceneLoaderThread.joinable()) mSceneLoaderThread.join();
	// remove old scene from file watcher
	FileWatcher::getInstance().removeFile(mRenderSystem.getMainScene()->getSceneFileName());
	// delete old scene
	mRenderSystem.getMainScene()->readyToRender(false);
	mRenderSystem.sceneUnload(mRenderSystem.getMainScene()->getSceneData());
	mRenderSystem.freeRenderScene(mRenderSystem.getMainScene());
	// unselect object before switching
	mRenderSystem.getMainScene()->setSelection(nullptr);
	// load new
	RenderScene* scene = mRenderSystem.allocRenderScene();
	mRenderSystem.setMainRenderScene(scene);
	mRenderSystem.sceneLoad(scene->getSceneData());
	scene->readyToRender(true);
}
void Common::openScene(const std::string& aFile)
{
	// wait for previous scene loader thread
	if(mSceneLoaderThread.joinable()) mSceneLoaderThread.join();
	// remove old scene from file watcher
	FileWatcher::getInstance().removeFile(mRenderSystem.getMainScene()->getSceneFileName());
	// delete old scene
	mRenderSystem.getMainScene()->readyToRender(false);
	mRenderSystem.sceneUnload(mRenderSystem.getMainScene()->getSceneData());
	mRenderSystem.freeRenderScene(mRenderSystem.getMainScene());
	// unselect object before switching
	mRenderSystem.getMainScene()->setSelection(nullptr);

	// load new
	RenderScene* scene = mRenderSystem.allocRenderScene();
	mSceneLoaderThread = std::thread([scene, aFile, this]() {
		if (scene->initFromFile(aFile)) {
			mRenderSystem.sceneLoad(scene->getSceneData());

			// add to file watcher
			FileWatcher::getInstance().watchFile(aFile, [scene, aFile, this]() {
				// save old cam name
				const std::string cam = scene->getCurrentCamera()->camera->getName();
				// remove scene data from gpu/backend
				scene->readyToRender(false);
				mRenderSystem.sceneUnload(scene->getSceneData());
				// destroy scene
				scene->destroy();
				// reload scene
				scene->initFromFile(aFile);
				// restore old cam
				for (RefCamera_s* c : *scene->getAvailableCameras()) {
					if (cam == c->camera->getName()) { scene->setCurrentCamera(c); break; }
				}
				// upload to gpu
				mRenderSystem.sceneLoad(scene->getSceneData());
				scene->readyToRender(true);
				});
		}
		scene->readyToRender(true);
	});
	if(!var::async_scene_loading.getBool() && mSceneLoaderThread.joinable()) mSceneLoaderThread.join();
	mRenderSystem.setMainRenderScene(scene);
}

void Common::addScene(const std::string& aFile) const
{
	RenderScene* rs = mRenderSystem.getMainScene();
	rs->readyToRender(false);
	mRenderSystem.sceneUnload(rs->getSceneData());
	if(!rs->addSceneFromFile(aFile))
	{
		spdlog::error("Could not add scene");
	}
	rs->readyToRender(true);
	mRenderSystem.sceneLoad(rs->getSceneData());
}

void Common::addModel(const std::string& aFile) const
{
	Model *m = Importer::load_model(aFile);
	glm::quat quat = glm::quat(glm::radians(glm::vec3(-90, 0, 0)));
	mRenderSystem.getMainScene()->addModelRef(m, glm::vec3(0), glm::make_vec4(&quat[0]));
	// TODO: do not reload whole scene when something is added
	mRenderSystem.reloadCurrentScene();
}
void Common::addLight(const std::string& aFile) const
{
	Light *l = Importer::load_light(aFile);
	glm::quat quat = glm::quat(glm::radians(glm::vec3(-90, 0, 0)));
	mRenderSystem.getMainScene()->addLightRef(l, glm::vec3(0), glm::make_vec4(&quat[0]));
	
}

void Common::exportScene(const std::string& aOutputFile, const uint32_t aSettings) const
{
	const Exporter::SceneExportSettings exportSettings(aSettings);
	const SceneInfo_s si = mRenderSystem.getMainScene()->getSceneInfo();
	Exporter::save_scene(aOutputFile, exportSettings, si);
}

void Common::reloadBackendImplementation() const
{
	mRenderSystem.reloadBackendImplementation();
}

void Common::changeBackendImplementation(const int aIdx)
{
	mIntersectionSettings = IntersectionSettings();
	mRenderSystem.getMainScene()->setSelection(nullptr); // unselect object before switching
	mRenderSystem.changeBackendImplementation(aIdx);	// change to backend implementation
}

void tamashii::Common::clearCache() const
{
	std::filesystem::remove_all(var::cache_dir.getString());
	std::filesystem::create_directories(var::cache_dir.getString() + "/shader");
	spdlog::info("Cache cleared: '{}'", var::cache_dir.getString());
}

void Common::takeScreenshot(const std::filesystem::path& aOut, const bool aScreenshotNoUi)
{
	mScreenshotPath = aOut;
	mScreenshotNoUi = aScreenshotNoUi;
}

void Common::intersectScene(const IntersectionSettings aSettings, HitInfo_s* aHitInfo)
{
	RenderScene* scene = mRenderSystem.getMainScene();
	if (!scene) return;
	const glm::vec2 mouse_coordinates = InputSystem::getInstance().getMousePosAbsolute();
	const glm::vec2 uv = (mouse_coordinates + glm::vec2(0.5f)) / glm::vec2(mWindow.getSize());
	const glm::vec2 screenCoordClipSpace = uv * glm::vec2(2.0) - glm::vec2(1.0);

	auto* cam = reinterpret_cast<RefCameraPrivate_s*>(scene->getCurrentCamera());
	glm::vec4 dirViewSpace = glm::inverse(cam->camera->getProjectionMatrix()) * glm::vec4(screenCoordClipSpace, 1, 1);
	dirViewSpace = glm::vec4(glm::normalize(glm::vec3(dirViewSpace)), 0);
	const auto dirWorldSpace = glm::vec3(glm::inverse(cam->view_matrix) * dirViewSpace);

	const glm::vec3 posWorldSpace = cam->getPosition();
	aHitInfo->mOriginPos = posWorldSpace;
	scene->intersect(posWorldSpace, dirWorldSpace, aSettings, aHitInfo);
}

CmdSystem* Common::getCmdSystem()
{ return &mCmdSystem; }

VarSystem* Common::getVarSystem()
{ return &mVarSystem; }

RenderSystem* Common::getRenderSystem()
{ return &mRenderSystem; }

CmdArgs* Common::getCommandLineArguments()
{ return &mCmdArgs; }

WindowThread* Common::getMainWindow()
{ return var::headless.getBool() ? nullptr : &mWindow; }

IntersectionSettings& Common::intersectionSettings()
{
	return mIntersectionSettings;
}

void Common::openFileDialogOpenScene()
{
	if (mFileDialogRunning.load()) return;
	mFileDialogRunning.store(true);
	EventSystem::getInstance().reset();
	auto t = std::thread([]() {
		const std::filesystem::path path = sys::openFileDialog("Open Scene", Importer::instance().load_scene_file_dialog_info());
		if (!path.string().empty()) EventSystem::queueEvent(EventType::ACTION, Input::A_OPEN_SCENE, 0, 0, 0, path.string());
		mFileDialogRunning.store(false);
	});
	t.detach();
}

void Common::openFileDialogAddScene()
{
	if (mFileDialogRunning.load()) return;
	mFileDialogRunning.store(true);
	EventSystem::getInstance().reset();
	auto t = std::thread([]() {
		const std::filesystem::path path = sys::openFileDialog("Add Scene", Importer::instance().load_scene_file_dialog_info());
		if (!path.string().empty()) EventSystem::queueEvent(EventType::ACTION, Input::A_ADD_SCENE, 0, 0, 0, path.string());
		mFileDialogRunning.store(false);
	});
	t.detach();
}

void Common::openFileDialogOpenModel()
{
	if (mFileDialogRunning.load()) return;
	mFileDialogRunning.store(true);
	EventSystem::getInstance().reset();
	auto t = std::thread([]() {
		const std::filesystem::path path = sys::openFileDialog("Add Model", { { "Model", {"*.ply", "*.obj"} },
																   { "PLY (.ply)", {"*.ply"} },
																   { "OBJ (.obj)", {"*.obj"} } });
		if (!path.string().empty()) EventSystem::queueEvent(EventType::ACTION, Input::A_ADD_MODEL, 0, 0, 0, path.string());
		mFileDialogRunning.store(false);
	});
	t.detach();
}

void Common::openFileDialogOpenLight()
{
	if (mFileDialogRunning.load()) return;
	mFileDialogRunning.store(true);
	EventSystem::getInstance().reset();
	auto t = std::thread([]() {
		const std::filesystem::path path = sys::openFileDialog("Add Light", { { "Light", {"*.ies", "*.ldt"} },
																   { "IES (.ies)", {"*.ies"} },
																   { "LDT (.ldt)", {"*.ldt"} } });
		if (!path.string().empty()) EventSystem::queueEvent(EventType::ACTION, Input::A_ADD_LIGHT, 0, 0, 0, path.string());
		mFileDialogRunning.store(false);
	});
	t.detach();
}

void Common::openFileDialogExportScene(const uint32_t aSettings)
{
	if (mFileDialogRunning.load()) return;
	mFileDialogRunning.store(true);
	EventSystem::getInstance().reset();
	const Exporter::SceneExportSettings exportSettings(aSettings);
	auto t = std::thread([exportSettings]() {
		std::string location;
		switch (exportSettings.mFormat) {
		case Exporter::SceneExportSettings::Format::glTF:
		{
			if (exportSettings.mWriteBinary) location = sys::saveFileDialog("Save Scene", "glb").string();
			else location = sys::saveFileDialog("Save Scene", "gltf").string();
			break;
		}
		}
		if (!location.empty()) EventSystem::queueEvent(EventType::ACTION, Input::A_EXPORT_SCENE, static_cast<int>(exportSettings.encode()), 0, 0, location);
		mFileDialogRunning.store(false);
	});
	t.detach();
}

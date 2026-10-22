#include <tamashii/renderer_vk/render_backend.hpp>
// renderer
#include <rvk/rvk.hpp>
#include <rvk/shader_compiler.hpp>
// callback
#include <tamashii/engine/common/common.hpp>
#include <tamashii/engine/platform/filewatcher.hpp>
#include <tamashii/engine/common/vars.hpp>
#include <tamashii/engine/render/render_backend_implementation.hpp>
// gui
#include <tamashii/gui/imgui_gui.hpp>
#include "gui/imgui_vulkan.hpp"

T_USE_NAMESPACE
RVK_USE_NAMESPACE

VulkanRenderBackend::VulkanRenderBackend() : mSceneLoaded(false), mCmdRecording(false),
	mCurrentImplementationIndex(0), mActiveGui(false)
{
}

VulkanRenderBackend::~VulkanRenderBackend()
{
	for (const RenderBackendImplementation* impl : mImplementations) {
		delete impl;
	}
}

void VulkanRenderBackend::init(Window* aMainWindow) {

	std::vector<Window*> windows;
	if (aMainWindow) {
		aMainWindow->init(var::window_title.getString().c_str(), var::window_size.getInt2(), var::window_pos.getInt2());
		windows.push_back(aMainWindow);
	}

	// start online shader compiler
	scomp::init();
	mInstance.init(windows);

	mInstance.setResizeCallback([this](int aWidth, int aHeight, rvk::Swapchain* sc) {
        Common::getInstance().getMainWindow()->setSize({aWidth, aHeight});
        rvk::RPipeline::global_render_state.scissor.extent.width = aWidth;
        rvk::RPipeline::global_render_state.scissor.extent.height = aHeight;
        rvk::RPipeline::global_render_state.viewport.width = aWidth;
        rvk::RPipeline::global_render_state.viewport.height = aHeight;

        if(!mImplementations.empty()) mImplementations[mCurrentImplementationIndex]->windowSizeChanged(aWidth, aHeight);
    });
	spdlog::info("...done");
	mInstance.printDeviceInfos();

	mActiveGui = aMainWindow;
	if (mActiveGui) mInstance.initImGui();

	// set shader cache
	rvk::Shader::cache = true;
	rvk::Shader::cache_dir = var::cache_dir.getString() + "/shader";
	rvk::Pipeline::pcache = true;
	rvk::Pipeline::cache_dir = var::cache_dir.getString();

	// compiler
	int major, minor, patch;
	scomp::getGlslangVersion(major, minor, patch);
	spdlog::info("Glslang Version: {}.{}.{}", major, minor, patch);
	scomp::getDxcVersion(major, minor, patch);
	spdlog::info("Dxc Version: {}.{}.{}", major, minor, patch);

	// set init and shutdown calls for glslang 
	FileWatcher::getInstance().setInitCallback([]() { scomp::init(); });
	FileWatcher::getInstance().setShutdownCallback([]() { scomp::finalize(); });

	mImplementations = initImplementations(&mInstance);
	for (size_t i = 0; i < mImplementations.size(); i++) {
		if (mImplementations[i]->getName() == var::default_implementation.getString()) mCurrentImplementationIndex = i;
	}

	prepare();
	if (aMainWindow) aMainWindow->showWindow(var::window_maximized.getBool() ? Window::Show::max : Window::Show::yes);
}
void VulkanRenderBackend::shutdown()
{
	spdlog::info("...shutting down Vulkan");
	// make sure we wait for all gpu operations to finish
	if(mInstance.mDevice) mInstance.mDevice->waitIdle();

	// delete
	destroy();
	if (mActiveGui) mInstance.destroyImGui();
	mInstance.destroy();
	// finalize online shader compiler
	scomp::finalize();
}

void VulkanRenderBackend::addImplementation(RenderBackendImplementation* aImplementation)
{
	if (!mImplementations.empty()) mImplementations.push_back(aImplementation);
}

void VulkanRenderBackend::reloadImplementation(scene_s aScene)
{
	// make sure we wait for all gpu operations to finish
	mInstance.mDevice->waitIdle();
	sceneUnload(aScene);
	destroy();
	spdlog::info("Reloading backend implementation: {}", mImplementations[mCurrentImplementationIndex]->getName());
	prepare();
	sceneLoad(aScene);
}

void VulkanRenderBackend::changeImplementation(const uint32_t aIndex, const scene_s aScene)
{
	// make sure we wait for all gpu operations to finish
	mInstance.mDevice->waitIdle();
	sceneUnload(aScene);
	destroy();
	mCurrentImplementationIndex = aIndex;
	spdlog::info("Changing backend implementation to: {}", mImplementations[mCurrentImplementationIndex]->getName());
	prepare();
	sceneLoad(aScene);
}

std::vector<RenderBackendImplementation*>& VulkanRenderBackend::getAvailableBackendImplementations()
{ return mImplementations; }

RenderBackendImplementation* VulkanRenderBackend::getCurrentBackendImplementations() const
{ return !mImplementations.empty() ? mImplementations[mCurrentImplementationIndex] : nullptr; }

void VulkanRenderBackend::recreateRenderSurface(const uint32_t aWidth, const uint32_t aHeight)
{
	mInstance.recreateSwapchain(aWidth, aHeight);
	const VkExtent2D extent = mInstance.mSwapchainData.mSwapchain->getExtent();

    rvk::RPipeline::global_render_state.scissor.extent.width = extent.width;
    rvk::RPipeline::global_render_state.scissor.extent.height = extent.height;
	rvk::RPipeline::global_render_state.viewport.width = extent.width;
	rvk::RPipeline::global_render_state.viewport.height = extent.height;

	if (!mImplementations.empty()) mImplementations[mCurrentImplementationIndex]->windowSizeChanged(extent.width, extent.height);
}

void VulkanRenderBackend::entitiyAdded(const Ref_s* aRef) const
{
	if (!mImplementations.empty()) mImplementations[mCurrentImplementationIndex]->entityAdded(aRef);
}

void VulkanRenderBackend::entitiyRemoved(const Ref_s* aRef) const
{
	if (!mImplementations.empty()) mImplementations[mCurrentImplementationIndex]->entityRemoved(aRef);
}

bool VulkanRenderBackend::drawOnMesh(const DrawInfo_s* aDrawInfo) const
{
	if (mImplementations.empty()) return true;
	return mImplementations[mCurrentImplementationIndex]->drawOnMesh(aDrawInfo);
}

void VulkanRenderBackend::screenshot(const std::string& aName) const
{
	if (mImplementations.empty()) return;
	return mImplementations[mCurrentImplementationIndex]->screenshot(aName);
}

void VulkanRenderBackend::sceneLoad(const scene_s aScene) {
	if (!mImplementations.empty()) {
		mImplementations[mCurrentImplementationIndex]->sceneLoad(aScene);
		mSceneLoaded.store(true);
	}
}

void VulkanRenderBackend::sceneUnload(const scene_s aScene) {
	if (!mImplementations.empty()) {
		mSceneLoaded.store(false);
		// make sure that no other thread is currently recording a cmd buffer with the data that we want to delete now
		while (mCmdRecording.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
		// make sure we wait for all gpu operations to finish
		mInstance.mDevice->waitIdle();
		mImplementations[mCurrentImplementationIndex]->sceneUnload(aScene);
	}
}

void VulkanRenderBackend::beginFrame() {
	mCmdRecording.store(true);
	mInstance.beginFrame();
}

void VulkanRenderBackend::drawView(viewDef_s* aViewDef)
{
	aViewDef->headless = var::headless.getBool();
	if (aViewDef->headless) aViewDef->target_size = var::render_size.getInt2();
	else {
		//const VkExtent2D extent = cmd::getSwapchain()->getExtent();
		const VkExtent2D extent = mInstance.mSwapchainData.mSwapchain->getExtent();
		aViewDef->target_size = glm::ivec2(extent.width, extent.height);
	}
	if (mSceneLoaded.load() && !mImplementations.empty()) mImplementations[mCurrentImplementationIndex]->drawView(aViewDef);
}

void VulkanRenderBackend::drawUI(uiConf_s* aUiConf)
{
	if (!mInstance.mSwapchainData.mSwapchain) return;
	ImGui_Vulkan_Impl::prepare(mInstance.mSwapchainData.mSwapchain->getExtent());
	if (!var::hide_default_gui.getBool()) mMainGui.draw(aUiConf);
	if (((RenderSystem*)aUiConf->system)->getConfig().show_gui && !mImplementations.empty()) mImplementations[mCurrentImplementationIndex]->drawUI(aUiConf);

	VulkanInstance::SwapchainData sd = mInstance.mSwapchainData;
	VulkanInstance::SwapchainImageData id = sd.mImages[sd.mCurrentImageIndex];
	CommandBuffer* cb = id.mCommandBuffer;
	Swapchain* sc = sd.mSwapchain;
	if (!mSceneLoaded.load()) {
		const glm::vec3 cc = glm::vec3(var::bg.getInt3()) / 255.0f;
		sc->CMD_BeginRenderClear(cb, { { cc.x, cc.y, cc.z, 1.0f }, {0,0} });
	}
	else sc->CMD_BeginRenderKeep(cb);

	ImGui_Vulkan_Impl::draw(cb);
	sc->CMD_EndRender(cb);
}

void VulkanRenderBackend::captureSwapchain(screenshotInfo_s* aScreenshotInfo)
{
	const bool alpha = false;
	const VkExtent2D extent = mInstance.mSwapchainData.mSwapchain->getExtent();
	aScreenshotInfo->width = extent.width;
	aScreenshotInfo->height = extent.height;
	aScreenshotInfo->channels = alpha ? 4 : 3;
	SingleTimeCommand stc(mInstance.mCommandPools[0], mInstance.mDevice->getQueue(0,1));
	aScreenshotInfo->data = rvk::swapchain::readPixelsScreen(&stc, mInstance.mSwapchainData.mSwapchain, alpha);
}

void VulkanRenderBackend::endFrame() {
	//rvk::render::endFrame();
	mInstance.endFrame();
	mCmdRecording.store(false);
}

void VulkanRenderBackend::prepare() const
{
	renderInfo_s ri{};
	ri.headless = var::headless.getBool();
	if (ri.headless) ri.target_size = var::render_size.getInt2();
	else {
		const VkExtent2D extent = mInstance.mSwapchainData.mSwapchain->getExtent();
		ri.target_size = glm::ivec2(extent.width, extent.height);
	}

	if (!mImplementations.empty()) mImplementations[mCurrentImplementationIndex]->prepare(&ri);
}

void VulkanRenderBackend::destroy() const
{
	if (!mImplementations.empty()) mImplementations[mCurrentImplementationIndex]->destroy();
}

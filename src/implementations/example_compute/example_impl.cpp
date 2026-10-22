#include "example_impl.hpp"
#include <tamashii/tamashii.hpp>
#include <tamashii/engine/common/common.hpp>
#include <rvk/rvk.hpp>
#include <imgui.h>

T_USE_NAMESPACE
RVK_USE_NAMESPACE

// custom command line vars
#include <tamashii/engine/common/vars.hpp>
Var example_var("example_var", "100", Var::Flag::INTEGER | Var::Flag::INIT, "Set a var, and init it with -example_var value", "set_var");

// engine file watcher
#include <tamashii/engine/platform/filewatcher.hpp>
// e.g.
// tFileWatcher::getInstance().watchFile("assets/shader/rasterizer_default/texture.vert", [this]() { shader.reloadShader("assets/shader/rasterizer_default/texture.vert"); });
// tFileWatcher::getInstance().removeFile("assets/shader/rasterizer_default/texture.vert");

// input
#include <tamashii/engine/common/input.hpp>
// e.g.
// if(inputSystem.wasPressed(tInput::KEY_K)) spdlog::info("K pressed");

// convert engine confs to rvk confs
#include <tamashii/renderer_vk/convenience/rvk_type_converter.hpp>

exampleComputeImpl::exampleComputeImpl(rvk::LogicalDevice* aDevice, rvk::Swapchain* aSwapchain,
                                       std::function<rvk::CommandBuffer*()> aGetCurrentCmdBuffer,
                                       std::function<rvk::SingleTimeCommand()> aGetStCmdBuffer):
	mDevice(aDevice), mSwapchain(aSwapchain), mGetCurrentCmdBuffer(std::move(aGetCurrentCmdBuffer)),
	mGetStCmdBuffer(std::move(aGetStCmdBuffer))
{}

// window size changes
void exampleComputeImpl::windowSizeChanged(const int aWidth, const int aHeight) {

}

// backend init
void exampleComputeImpl::prepare(tamashii::renderInfo_s* aRenderInfo) {
	rvk::SingleTimeCommand stc = mGetStCmdBuffer();
	// create buffer on gpu 
	uint32_t dataArrayIn[] = { 99, 100, 101, 1 };
	const rvk::Buffer dataBuffer(mDevice, rvk::Buffer::Use::STORAGE | rvk::Buffer::Use::UPLOAD | rvk::Buffer::Use::DOWNLOAD, sizeof(dataArrayIn), rvk::Buffer::Location::DEVICE);
	dataBuffer.STC_UploadData(&stc, &dataArrayIn[0], sizeof(dataArrayIn));
	// create compute shader
	rvk::CShader cshader(mDevice);
	// compile shader from glsl/hlsl with glslang compiler or hlsl with directX compiler
	//cshader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::COMPUTE, "assets/shader/example_impl/simple_compute_glsl.comp");
	//cshader.addStage(rvk::Shader::Source::HLSL, rvk::Shader::Stage::COMPUTE, "assets/shader/example_impl/simple_compute.hlsl");
	cshader.addStage(rvk::Shader::Source::HLSL_DXC, rvk::Shader::Stage::COMPUTE, "assets/shader/example_impl/simple_compute.hlsl");
	cshader.finish();
	// set pipeline constant
	int multiplier = 5;
	cshader.addConstant(0, 0, sizeof(multiplier), 0);
	cshader.setConstantData(0, &multiplier, 4);
	// create descriptor
	rvk::Descriptor descriptor(mDevice);
	descriptor.addStorageBuffer(0, rvk::Shader::Stage::COMPUTE);
	descriptor.setBuffer(0, &dataBuffer);
	descriptor.finish();
	// create compute pipeline with shader
	rvk::CPipeline cpipe(mDevice);
	cpipe.addDescriptorSet({ &descriptor });
	cpipe.setShader(&cshader);
	cpipe.finish();

	// begin single time command and execute compute shader now
	stc.begin();
	cpipe.CMD_BindDescriptorSets(stc.buffer(), { &descriptor });
	cpipe.CMD_BindPipeline(stc.buffer());
	cpipe.CMD_Dispatch(stc.buffer(), 4);
	stc.end();

	// download results
	uint32_t dataArrayOut[4];
	dataBuffer.STC_DownloadData(&stc, &dataArrayOut[0], sizeof(dataArrayOut));
	spdlog::info("	Compute shader example: multiply each value of input array with {}", multiplier);
	spdlog::info("	input: {} {} {} {}", dataArrayIn[0], dataArrayIn[1], dataArrayIn[2], dataArrayIn[3]);
	spdlog::info("	result: {} {} {} {}", dataArrayOut[0], dataArrayOut[1], dataArrayOut[2], dataArrayOut[3]);

    //EventSystem::queueEvent(EventType::ACTION, Input::A_EXIT);
}
void exampleComputeImpl::destroy() {
	
}

// scene load/unload
void exampleComputeImpl::sceneLoad(scene_s aScene) {
	
}
void exampleComputeImpl::sceneUnload(scene_s aScene) {
	
}

// draw frame
void exampleComputeImpl::drawView(viewDef_s* aViewDef) {
	// get the command buffer for the current frame
	const CommandBuffer* cb = mGetCurrentCmdBuffer();
	if (mSwapchain) {
		// placeholder so the frame gets cleaned each frame
		const glm::vec3 cc = glm::vec3(var::bg.getInt3()) / 255.0f;
        mSwapchain->CMD_BeginRenderClear(cb, {{ cc.x, cc.y, cc.z, 1.0f }, {0, 0}});
		mSwapchain->CMD_EndRender(cb);
	}
}
void exampleComputeImpl::drawUI(uiConf_s* aUiConf) {
	if (ImGui::Begin("Settings", nullptr, 0))
	{
		ImGui::Separator();
		ImGui::TextWrapped("Add to the default UI or create your own widget");
		ImGui::Separator();
		ImGui::End();
	}
}

#include <tamashii/implementations/rasterizer_default.hpp>
#include <tamashii/implementations/raytracing_default.hpp>
class VulkanRenderBackendExampleCompute final : public tamashii::VulkanRenderBackend {
public:
	std::vector<tamashii::RenderBackendImplementation*> initImplementations(VulkanInstance* aInstance) override
	{
		// current swapchain command buffer
		const std::function getCurrentCmdBuffer = [aInstance]()->CommandBuffer*
		{
			return aInstance->mSwapchainData.mImages[aInstance->mSwapchainData.mCurrentImageIndex].mCommandBuffer;
		};
		// single time command buffer for two threads
		const std::function getStlCBuffer = [aInstance]()->SingleTimeCommand
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
			new exampleComputeImpl(aInstance->mDevice, aInstance->mSwapchainData.mSwapchain,getCurrentCmdBuffer, getStlCBuffer)
		};
	}
};

int main(int argc, char* argv[]) {
	// use our vulkan backend
	tamashii::addBackend(new VulkanRenderBackendExampleCompute());
	var::default_implementation.setValue("Example Compute");
	// start
	run(argc, argv);
}

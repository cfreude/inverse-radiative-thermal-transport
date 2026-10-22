#include <tamashii/implementations/raytracing_default.hpp>
#include <imgui.h>
// engine imports
#include <tamashii/engine/common/common.hpp>
#include <tamashii/engine/common/vars.hpp>
#include <tamashii/engine/common/input.hpp>
#include <tamashii/engine/exporter/exporter.hpp>
#include <tamashii/engine/platform/filewatcher.hpp>

#include <glm/gtc/color_space.hpp>

T_USE_NAMESPACE
RVK_USE_NAMESPACE

namespace {
	Var renderer_samples("renderer_samples", "-1", Var::Flag::INTEGER | Var::Flag::CONFIG_RD, "Number of samples to reach before quitting", "set_var");
	Var pixel_samples("pixel_samples", "1", Var::Flag::INTEGER | Var::Flag::INIT | Var::Flag::CONFIG_RD, "Number of samples per pixel per frame", "set_var");
	Var max_depth("max_depth", "1", Var::Flag::INTEGER | Var::Flag::INIT | Var::Flag::CONFIG_RD, "Set number of indirect bounces", "set_var");
	Var output_filename("output_filename", "out", Var::Flag::STRING | Var::Flag::INIT, "Ray traced results output name", "set_var");
}

constexpr uint32_t MAX_GLOBAL_IMAGE_SIZE = 256;
constexpr uint32_t MAX_MATERIAL_SIZE = (3 * 1024);

void raytracingDefault::windowSizeChanged(const int aWidth, const int aHeight) {
	SingleTimeCommand stc = mGetStcBuffer();
	stc.begin();
	mGlobalUbo.accumulatedFrames = 0;
	// accumulate Image
	mVkData->rtImageAccumulate.destroy();
	mVkData->rtImageAccumulateCount.destroy();
	// rtImage
	mVkData->rtImageAccumulate.createImage2D(aWidth, aHeight, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	mVkData->rtImageAccumulate.CMD_TransitionImage(stc.buffer(), VK_IMAGE_LAYOUT_GENERAL);
	mVkData->rtImageAccumulateCount.createImage2D(aWidth, aHeight, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	mVkData->rtImageAccumulateCount.CMD_TransitionImage(stc.buffer(), VK_IMAGE_LAYOUT_GENERAL);


	for (uint32_t si_idx = 0; si_idx < mFrameCount; si_idx++) {
		VkFrameData& frameData = mVkFrameData[si_idx];

		frameData.rtImage.destroy();
		frameData.debugImage.destroy();
		// rtImage
		frameData.rtImage.createImage2D(aWidth, aHeight, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		frameData.rtImage.CMD_TransitionImage(stc.buffer(), VK_IMAGE_LAYOUT_GENERAL);
		frameData.debugImage.createImage2D(aWidth, aHeight, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		frameData.debugImage.CMD_TransitionImage(stc.buffer(), VK_IMAGE_LAYOUT_GENERAL);
		frameData.globalDescriptor.setImage(GLSL_GLOBAL_RT_OUT_IMAGE_BINDING, &frameData.rtImage);
		frameData.globalDescriptor.setImage(GLSL_GLOBAL_RT_ACC_IMAGE_BINDING, &mVkData->rtImageAccumulate);
		frameData.globalDescriptor.setImage(GLSL_GLOBAL_RT_ACC_C_IMAGE_BINDING, &mVkData->rtImageAccumulateCount);
		frameData.globalDescriptor.setImage(GLSL_GLOBAL_DEBUG_IMAGE_BINDING, &frameData.debugImage);
		if(mGpuTlas[si_idx].size()) frameData.globalDescriptor.update();
	}
	stc.end();
}

void raytracingDefault::screenshot(const std::string& aFilename)
{
	/*const VkExtent3D extent = mVkFrameData[mPrevFrame].rtImage.getExtent();
	std::vector<glm::u8vec4> data_uint8(extent.width * extent.height);
	SingleTimeCommand stc = mGetStcBuffer();
	mVkFrameData[mPrevFrame].rtImage.STC_DownloadData2D(&stc, extent.width, extent.height, 4, data_uint8.data());
	std::vector<glm::u8vec4> data_image;
	data_image.reserve(extent.width * extent.height);
	for(const auto v : data_uint8) data_image.emplace_back(glm::u8vec4(v.z, v.y, v.x, v.w));
	Exporter::save_image_png_8_bit(aFilename + ".png", extent.width, extent.height, 4, (uint8_t*)data_image.data());
	spdlog::info("Impl-Screenshot saved: {}.png", aFilename);*/

	const VkExtent3D extent = mVkData->rtImageAccumulate.getExtent();
	std::vector<glm::vec4> dataImage(extent.width * extent.height);
	SingleTimeCommand stc = mGetStcBuffer();
	mVkData->rtImageAccumulate.STC_DownloadData2D(&stc, extent.width, extent.height, 16, dataImage.data());
	for (auto& v : dataImage) v = glm::convertLinearToSRGB(v / static_cast<float>(mGlobalUbo.accumulatedFrames));
	Exporter::save_image_exr(aFilename + ".exr", extent.width, extent.height, reinterpret_cast<float*>(dataImage.data()), 4, { 2, 1, 0 } /*B G R*/);
	spdlog::info("Impl-Screenshot saved: {}.exr", aFilename);
}

// implementation preparation
void raytracingDefault::prepare(renderInfo_s* aRenderInfo) {
	mGlobalUbo.pixelSamplesPerFrame = pixel_samples.getInt();
	mGlobalUbo.max_depth = max_depth.getInt();

	mGpuTd.prepare(rvk::Shader::Stage::RAYGEN | rvk::Shader::Stage::ANY_HIT, MAX_GLOBAL_IMAGE_SIZE);
	mGpuTlas.resize(mFrameCount, mDevice);
	mUpdates.resize(mFrameCount);
	mGpuMd.prepare(rvk::Buffer::Use::STORAGE, MAX_MATERIAL_SIZE, true);
	mGpuLd.prepare(rvk::Buffer::Use::STORAGE);

	mVkData.emplace(mDevice);
	mVkFrameData.resize(mFrameCount, VkFrameData(mDevice));
	int count = 0;
	
	// ray tracing accumulation image
	SingleTimeCommand stc = mGetStcBuffer();
	stc.begin();
	mVkData->rtImageAccumulate.createImage2D(aRenderInfo->target_size.x, aRenderInfo->target_size.y, VK_FORMAT_R32G32B32A32_SFLOAT, rvk::Image::Use::DOWNLOAD | rvk::Image::Use::UPLOAD | rvk::Image::Use::STORAGE);
	mVkData->rtImageAccumulate.CMD_TransitionImage(stc.buffer(), VK_IMAGE_LAYOUT_GENERAL);
	mVkData->rtImageAccumulateCount.createImage2D(aRenderInfo->target_size.x, aRenderInfo->target_size.y, VK_FORMAT_R32_SFLOAT, rvk::Image::Use::DOWNLOAD | rvk::Image::Use::UPLOAD | rvk::Image::Use::STORAGE);
	mVkData->rtImageAccumulateCount.CMD_TransitionImage(stc.buffer(), VK_IMAGE_LAYOUT_GENERAL);

	// data for individual frames
	for (uint32_t si_idx = 0; si_idx < mFrameCount; si_idx++) {
		VkFrameData& frameData = mVkFrameData[si_idx];

		// ray tracing output image
		frameData.rtImage.createImage2D(aRenderInfo->target_size.x, aRenderInfo->target_size.y, VK_FORMAT_B8G8R8A8_UNORM, rvk::Image::Use::DOWNLOAD | rvk::Image::Use::UPLOAD | rvk::Image::Use::STORAGE);
		frameData.rtImage.CMD_TransitionImage(stc.buffer(), VK_IMAGE_LAYOUT_GENERAL);
		// ray tracing debug image
		frameData.debugImage.createImage2D(aRenderInfo->target_size.x, aRenderInfo->target_size.y, VK_FORMAT_B8G8R8A8_UNORM, rvk::Image::Use::DOWNLOAD | rvk::Image::Use::UPLOAD | rvk::Image::Use::STORAGE);
		frameData.debugImage.CMD_TransitionImage(stc.buffer(), VK_IMAGE_LAYOUT_GENERAL);
		// global uniform buffer
		frameData.globalUniformBuffer.create(rvk::Buffer::Use::UNIFORM, sizeof(GlobalUbo_s), rvk::Buffer::Location::HOST_COHERENT);
		frameData.globalUniformBuffer.mapBuffer();

		// descriptors
		// global descriptor
		frameData.globalDescriptor.reserve(8);
		frameData.globalDescriptor.addUniformBuffer(GLOBAL_DESC_UBO_BINDING, rvk::Shader::Stage::RAYGEN);
		frameData.globalDescriptor.addStorageBuffer(GLOBAL_DESC_GEOMETRY_DATA_BINDING, rvk::Shader::Stage::RAYGEN | rvk::Shader::Stage::ANY_HIT);
		frameData.globalDescriptor.addStorageBuffer(GLOBAL_DESC_INDEX_BUFFER_BINDING, rvk::Shader::Stage::RAYGEN | rvk::Shader::Stage::ANY_HIT);
		frameData.globalDescriptor.addStorageBuffer(GLOBAL_DESC_VERTEX_BUFFER_BINDING, rvk::Shader::Stage::RAYGEN | rvk::Shader::Stage::ANY_HIT);
		frameData.globalDescriptor.addStorageBuffer(GLOBAL_DESC_MATERIAL_BUFFER_BINDING, rvk::Shader::Stage::RAYGEN | rvk::Shader::Stage::ANY_HIT);
		frameData.globalDescriptor.addStorageBuffer(GLSL_GLOBAL_LIGHT_DATA_BINDING, rvk::Shader::Stage::RAYGEN | rvk::Shader::Stage::INTERSECTION);
		frameData.globalDescriptor.addAccelerationStructureKHR(GLOBAL_DESC_AS_BINDING, rvk::Shader::Stage::RAYGEN);
		frameData.globalDescriptor.addStorageImage(GLSL_GLOBAL_RT_OUT_IMAGE_BINDING, rvk::Shader::Stage::RAYGEN);
		frameData.globalDescriptor.addStorageImage(GLSL_GLOBAL_RT_ACC_IMAGE_BINDING, rvk::Shader::Stage::RAYGEN);
		frameData.globalDescriptor.addStorageImage(GLSL_GLOBAL_RT_ACC_C_IMAGE_BINDING, rvk::Shader::Stage::RAYGEN);
		frameData.globalDescriptor.addStorageImage(GLSL_GLOBAL_DEBUG_IMAGE_BINDING, rvk::Shader::Stage::RAYGEN);

		// set
		frameData.globalDescriptor.setBuffer(GLOBAL_DESC_UBO_BINDING, &frameData.globalUniformBuffer);
		frameData.globalDescriptor.setBuffer(GLOBAL_DESC_MATERIAL_BUFFER_BINDING, mGpuMd.getMaterialBuffer());
		frameData.globalDescriptor.setBuffer(GLSL_GLOBAL_LIGHT_DATA_BINDING, mGpuLd.getLightBuffer());
		frameData.globalDescriptor.setImage(GLSL_GLOBAL_RT_OUT_IMAGE_BINDING, &frameData.rtImage);
		frameData.globalDescriptor.setImage(GLSL_GLOBAL_RT_ACC_IMAGE_BINDING, &mVkData->rtImageAccumulate);
		frameData.globalDescriptor.setImage(GLSL_GLOBAL_RT_ACC_C_IMAGE_BINDING, &mVkData->rtImageAccumulateCount);
		frameData.globalDescriptor.setImage(GLSL_GLOBAL_DEBUG_IMAGE_BINDING, &frameData.debugImage);
		frameData.globalDescriptor.finish(false);
	}
	stc.end();
	// shading
	mVkData->rtshader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::RAYGEN, "assets/shader/raytracing_default/simple_ray.rgen", { "GLSL", "SAMPLE_LIGHTS" });
	mVkData->rtshader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::RAYGEN, "assets/shader/raytracing_default/simple_ray.rgen", { "GLSL", "SAMPLE_BRDF"});
	mVkData->rtshader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::RAYGEN, "assets/shader/raytracing_default/simple_ray.rgen", { "GLSL", "SAMPLE_LIGHTS_MIS" });
	mVkData->rtshader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::MISS, "assets/shader/raytracing_default/simple_ray.rmiss", { "GLSL" });
	mVkData->rtshader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::CLOSEST_HIT, "assets/shader/raytracing_default/simple_ray.rchit", { "GLSL" });
	mVkData->rtshader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::ANY_HIT, "assets/shader/raytracing_default/simple_ray.rahit", { "GLSL" });
	// sphere
	//vkData->rtshader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::INTERSECTION, "assets/shader/raytracing_default/sphere.rint", { "GLSL" });
	// shadow
	mVkData->rtshader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::MISS, "assets/shader/raytracing_default/shadow_ray.rmiss", { "GLSL" });
	mVkData->rtshader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::CLOSEST_HIT, "assets/shader/raytracing_default/shadow_ray.rchit", { "GLSL" });
	mVkData->rtshader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::ANY_HIT, "assets/shader/raytracing_default/shadow_ray.rahit", { "GLSL" });
	mVkData->rtshader.addGeneralShaderGroup("assets/shader/raytracing_default/simple_ray.rmiss"); // idx 0
	mVkData->rtshader.addGeneralShaderGroup("assets/shader/raytracing_default/shadow_ray.rmiss"); // idx 1
	mVkData->rtshader.addGeneralShaderGroup(0); // rgen idx 0
	mVkData->rtshader.addGeneralShaderGroup(1); // rgen idx 1
	mVkData->rtshader.addGeneralShaderGroup(2); // rgen idx 2
	// triangle
	mVkData->rtshader.addHitShaderGroup("assets/shader/raytracing_default/simple_ray.rchit", "assets/shader/raytracing_default/simple_ray.rahit"); // idx 0
	mVkData->rtshader.addHitShaderGroup("assets/shader/raytracing_default/shadow_ray.rchit", "assets/shader/raytracing_default/shadow_ray.rahit"); // idx 1
	// sphere
	//vkData->rtshader.addProceduralShaderGroup("assets/shader/raytracing_default/simple_ray.rchit", "",
	//												"assets/shader/raytracing_default/sphere.rint"); // idx 0
	//vkData->rtshader.addProceduralShaderGroup("assets/shader/raytracing_default/shadow_ray.rchit", "",
	//												"assets/shader/raytracing_default/sphere.rint"); // idx 1
	mVkData->rtshader.finish();
	
	tamashii::FileWatcher::getInstance().watchFile("assets/shader/raytracing_default/simple_ray.rgen", [this]() { mVkData->rtshader.reloadShader({0,1,2}); });
	//tFileWatcher::getInstance().watchFile("assets/shader/raytracing_default/sphere.rint", [this]() { vkData->rtshader.reloadShader("assets/shader/raytracing_default/sphere.rint"); });

	mVkData->rtpipeline.setShader(&mVkData->rtshader);
	mVkData->rtpipeline.addDescriptorSet({ mGpuTd.getDescriptor(), &mVkFrameData[0].globalDescriptor });
	mVkData->rtpipeline.finish();
}
void raytracingDefault::destroy() {
	mGpuTd.destroy();
	mGpuMd.destroy();
	mGpuLd.destroy();

	tamashii::FileWatcher::getInstance().removeFile("assets/shader/raytracing_default/simple_ray.rgen");

	mVkFrameData.clear();
	mVkData.reset();
}

// scene
void raytracingDefault::sceneLoad(scene_s aScene) {
	const GeometryDataVulkan::SceneInfo_s sinfo = GeometryDataVulkan::getSceneGeometryInfo(aScene);
	if (!sinfo.mMeshCount) {
		mGpuBlas.prepare();
		for (GeometryDataTlasVulkan& c : mGpuTlas) c.prepare(1, rvk::Buffer::Use::STORAGE, 1);
	}
	else {
		mGpuBlas.prepare(sinfo.mIndexCount, sinfo.mVertexCount);
		for (GeometryDataTlasVulkan& c : mGpuTlas) c.prepare(sinfo.mInstanceCount, rvk::Buffer::Use::STORAGE, sinfo.mGeometryCount);
	}

	SingleTimeCommand stc = mGetStcBuffer();
	mGpuTd.loadScene(&stc, aScene);
	mGpuBlas.loadScene(&stc, aScene);
	mGpuMd.loadScene(&stc, aScene, &mGpuTd);
	mGpuLd.loadScene(&stc, aScene, &mGpuTd, &mGpuBlas);
	for (GeometryDataTlasVulkan& tlas : mGpuTlas) tlas.loadScene(&stc, aScene, &mGpuBlas, &mGpuMd);
	for (uint32_t idx = 0; idx < mFrameCount; idx++) {
		mVkFrameData[idx].globalDescriptor.setBuffer(GLOBAL_DESC_GEOMETRY_DATA_BINDING, mGpuTlas[idx].getGeometryDataBuffer());
		mVkFrameData[idx].globalDescriptor.setBuffer(GLOBAL_DESC_INDEX_BUFFER_BINDING, mGpuBlas.getIndexBuffer());
		mVkFrameData[idx].globalDescriptor.setBuffer(GLOBAL_DESC_VERTEX_BUFFER_BINDING, mGpuBlas.getVertexBuffer());
		if(mGpuMd.bufferChanged(false)) mVkFrameData[idx].globalDescriptor.setBuffer(GLOBAL_DESC_MATERIAL_BUFFER_BINDING, mGpuMd.getMaterialBuffer());

		mVkFrameData[idx].globalDescriptor.setAccelerationStructureKHR(GLOBAL_DESC_AS_BINDING, mGpuTlas[idx].getTlas());
		mVkFrameData[idx].globalDescriptor.update();
	}

	// check for lights
	mGlobalUbo.shade = !aScene.refLights.empty();
	for (Model* model : aScene.models) {
		for (const Mesh* mesh : *model) {
			if (mesh->getMaterial()->isLight()) {
				mGlobalUbo.shade = true;
				break;
			}
		}
	}
}

void raytracingDefault::sceneUnload(scene_s aScene) {
	mGpuTd.unloadScene();
	mGpuBlas.unloadScene();
	for (GeometryDataTlasVulkan& c : mGpuTlas) {
		c.unloadScene();
	}
	mGpuMd.unloadScene();
	mGpuLd.unloadScene();

	for (GeometryDataTlasVulkan& c : mGpuTlas) c.destroy();
	mGpuBlas.destroy();
}

// frame
void raytracingDefault::drawView(viewDef_s* aViewDef) {
	if (renderer_samples.getInt() > 0) {
		mGlobalUbo.accumulate = true;
		if (static_cast<int>(mGlobalUbo.accumulatedFrames) > renderer_samples.getInt())
		{
			screenshot(output_filename.getString());
			EventSystem::queueEvent(EventType::ACTION, Input::A_EXIT);
			return;
		}
		else if (aViewDef->headless) spdlog::info("[{}/{}]", static_cast<int>(mGlobalUbo.accumulatedFrames), renderer_samples.getInt());
	}

	CommandBuffer* cb = mGetCurrentCmdBuffer();
	const uint32_t fi = mCurrentFrame();

	const glm::vec4 cc = glm::vec4(var::bg.getInt3(), 255.f) / 255.0f;
	if (aViewDef->surfaces.empty()) {
		mSwapchain->CMD_BeginRenderClear(cb, { { cc.x, cc.y, cc.z, 1.0f }, {0,0} });
		mSwapchain->CMD_EndRender(cb);
		return;
	};
	if (aViewDef->updates.any()) for (uint32_t i = 0; i < mFrameCount; i++) mUpdates[i] = mUpdates[i] | aViewDef->updates;

	// as updates
	if (mUpdates[fi].mModelInstances || mUpdates[fi].mMaterials) {
		//spdlog::debug("update {}", vd->frame_index);
		mUpdates[fi].mModelInstances = mUpdates[fi].mMaterials = false;
		SingleTimeCommand stc = mGetStcBuffer();
		mGpuMd.update(&stc, aViewDef->scene, &mGpuTd);
		mGpuTlas[fi].update(cb, &stc, aViewDef->scene, &mGpuBlas, &mGpuMd);

		cb->cmdMemoryBarrier(VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
		mVkFrameData[fi].globalDescriptor.setAccelerationStructureKHR(GLOBAL_DESC_AS_BINDING, mGpuTlas[fi].getTlas());
		mVkFrameData[fi].globalDescriptor.update();
	}
	if (aViewDef->updates.mImages || aViewDef->updates.mTextures) {
		mDevice->waitIdle();
		SingleTimeCommand stc = mGetStcBuffer();
		mGpuTd.update(&stc, aViewDef->scene);
	}
	if (aViewDef->updates.mLights) {
		SingleTimeCommand stc = mGetStcBuffer();
		mGpuLd.update(&stc, aViewDef->scene, &mGpuTd, &mGpuBlas);
	}
	if (aViewDef->updates.any() || mRecalculate) {
		mRecalculate = false;
		mGlobalUbo.accumulatedFrames = 0;
		mVkData->rtImageAccumulate.CMD_ClearColor(cb, 0, 0, 0, 0);
		mVkData->rtImageAccumulateCount.CMD_ClearColor(cb, 0, 0, 0, 0);
	}
	
	const glm::vec3 bg(var::bg.getInt3());
	mVkFrameData[mCurrentFrame()].rtImage.CMD_ClearColor(cb, bg.x / 255.0f, bg.y / 255.0f, bg.z / 255.0f ,1.0f);
	mVkFrameData[mCurrentFrame()].debugImage.CMD_ClearColor(cb, 0, 0, 0, 1.0f);

	static glm::vec2 debugClickPos = glm::vec2(0, 0);
	if (tamashii::InputSystem::getInstance().wasPressed(tamashii::Input::MOUSE_LEFT)) {
		debugClickPos = tamashii::InputSystem::getInstance().getMousePosAbsolute();
	}
	
	SingleTimeCommand stl = mGetStcBuffer();
	stl.begin();

	// ubo
	mGlobalUbo.viewMat = aViewDef->view_matrix;
	mGlobalUbo.projMat = aViewDef->projection_matrix;
	mGlobalUbo.inverseViewMat = aViewDef->inv_view_matrix;
	mGlobalUbo.inverseProjMat = aViewDef->inv_projection_matrix;
	mGlobalUbo.viewPos = glm::vec4(aViewDef->view_pos, 1);
	mGlobalUbo.viewDir = glm::vec4(aViewDef->view_dir, 0);
	mGlobalUbo.debugPixelPosition = debugClickPos;
	mGlobalUbo.cull_mode = mActiveCullMode;
	mGlobalUbo.env_shade = mEnvMapShading;
	Common::getInstance().intersectionSettings().mCullMode = static_cast<CullMode_e>(mActiveCullMode);
	mGlobalUbo.bg[0] = mEnvLight[0] * mEnvLightIntensity; mGlobalUbo.bg[1] = mEnvLight[1] * mEnvLightIntensity; mGlobalUbo.bg[2] = mEnvLight[2] * mEnvLightIntensity; mGlobalUbo.bg[3] = 1;
	mGlobalUbo.size[0] = static_cast<float>(aViewDef->target_size.x); mGlobalUbo.size[1] = static_cast<float>(aViewDef->target_size.y);
	mGlobalUbo.frameIndex = static_cast<float>(aViewDef->frame_index);
	mGlobalUbo.light_count = mGpuLd.getLightCount();
	// accumulation calc
	if (mGlobalUbo.accumulate) mGlobalUbo.accumulatedFrames += mGlobalUbo.pixelSamplesPerFrame;
	else mGlobalUbo.accumulatedFrames = mGlobalUbo.pixelSamplesPerFrame;
	mVkFrameData[mCurrentFrame()].globalUniformBuffer.STC_UploadData(&stl, &mGlobalUbo, sizeof(GlobalUbo_s));
	stl.end();
	

	if (!aViewDef->scene.refModels.empty()) {
		VkFrameData& frameData = mVkFrameData[mCurrentFrame()];

		mVkData->rtpipeline.CMD_BindDescriptorSets(cb, { mGpuTd.getDescriptor(), &mVkFrameData[mCurrentFrame()].globalDescriptor });
		mVkData->rtpipeline.CMD_BindPipeline(cb);
		mVkData->rtpipeline.CMD_TraceRays(cb, aViewDef->target_size.x, aViewDef->target_size.y,1, mGlobalUbo.sampling_strategy);
	}

	if (!aViewDef->headless) {
		if (!mDebugImage) rvk::swapchain::CMD_BlitImageToCurrentSwapchainImage(cb, mSwapchain, &mVkFrameData[mCurrentFrame()].rtImage, VK_FILTER_LINEAR);
		else rvk::swapchain::CMD_BlitImageToCurrentSwapchainImage(cb, mSwapchain, &mVkFrameData[mCurrentFrame()].debugImage, VK_FILTER_LINEAR);
	}
	mPrevFrame = mCurrentFrame();
}
void raytracingDefault::drawUI(uiConf_s* aUiConf) {
	if (ImGui::Begin("Settings", nullptr, 0))
	{
		ImGui::Separator();
		ImGui::Checkbox("Shade", reinterpret_cast<bool*>(&mGlobalUbo.shade));
		ImGui::SliderFloat("##dither", &mGlobalUbo.dither_strength, 0, 2.0f, "Dither: %.3f");
		ImGui::Checkbox("Accumulate", reinterpret_cast<bool*>(&mGlobalUbo.accumulate));
		ImGui::DragInt("##PPS", &mGlobalUbo.pixelSamplesPerFrame, 1, 1, 1000, "Pixel Samples per Frame: %d");
		ImGui::DragInt("##D", &mGlobalUbo.max_depth, 1, 0, 1000, "Max Depth: %d");
		ImGui::Checkbox("Debug View", &mDebugImage);
		ImGui::Checkbox("Debug Output", &mDebugOutput);

		if (ImGui::CollapsingHeader("Environment Light", 0)) {
			ImGui::Checkbox("Environment Shading: ", &mEnvMapShading);
			if (ImGui::ColorEdit3("Color", &mEnvLight[0], ImGuiColorEditFlags_NoInputs)) mRecalculate = true;
			ImGui::SliderFloat("##envlintens", &mEnvLightIntensity, 0, 10.0f, "Intensity: %.3f");
		}
		if (ImGui::CollapsingHeader("Sampling", 0)) {
			if (ImGui::BeginCombo("##scombo", mSampling[mGlobalUbo.sampling_strategy].c_str(), ImGuiComboFlags_NoArrowButton)) {
				for (uint32_t i = 0; i < mSampling.size(); i++) {
					const bool isSelected = (i == mGlobalUbo.sampling_strategy);
					if (ImGui::Selectable(mSampling[i].c_str(), isSelected)) {
						mGlobalUbo.sampling_strategy = i;
						mRecalculate = true;
					}
					if (isSelected) ImGui::SetItemDefaultFocus();
				}
					ImGui::EndCombo();
			}
		}
		if(ImGui::CollapsingHeader("Ray Settings", 0)){
			static float a = 0;
			ImGui::PushItemWidth(64);
			ImGui::Text("			  Tmin	  Tmax");
			ImGui::Text("First Ray: ");
			ImGui::SameLine();
			ImGui::DragFloat("##firstrayTmin", &mGlobalUbo.fr_tmin, 0.01f, 0, mGlobalUbo.fr_tmax, "%g");
			ImGui::SameLine();
			ImGui::DragFloat("##firstrayTmax", &mGlobalUbo.fr_tmax, 1.0, mGlobalUbo.fr_tmin, std::numeric_limits<float>::max(), "%g");
			ImGui::Text("Bounce Ray:");
			ImGui::SameLine();
			ImGui::DragFloat("##bouncerayTmin", &mGlobalUbo.br_tmin, 0.01f, 0, mGlobalUbo.br_tmax, "%g");
			ImGui::SameLine();
			ImGui::DragFloat("##bouncerayTmax", &mGlobalUbo.br_tmax, 1.0, mGlobalUbo.br_tmin, std::numeric_limits<float>::max(), "%g");
			ImGui::Text("			  Tmin     Offset");
			ImGui::Text("Shadow Ray:");
			ImGui::SameLine();
			ImGui::DragFloat("##shadowrayTmin", &mGlobalUbo.sr_tmin, 0.001f, 0.001f, std::numeric_limits<float>::max(), "%g");
			ImGui::SameLine();
			ImGui::DragFloat("##shadowrayTmax", &mGlobalUbo.sr_tmax_offset, 0.001f, 0, 0, "%g");
			ImGui::PopItemWidth();
			ImGui::Text("Cull Mode: "); ImGui::SameLine();
			ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
			if (ImGui::BeginCombo("##cmcombo", mCullMode[mActiveCullMode].c_str(), ImGuiComboFlags_NoArrowButton)) {
				for (uint32_t i = 0; i < mCullMode.size(); i++) {
					const bool isSelected = (i == mActiveCullMode);
					if (ImGui::Selectable(mCullMode[i].c_str(), isSelected)) mActiveCullMode = i;
					if (isSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::PopItemWidth();
			ImGui::DragInt("##rrpt", &mGlobalUbo.rrpt, 1, -1, 1000, "Russian Roulette PT: %d");
		}
		if (ImGui::CollapsingHeader("Pixel Filter", 0)) {
			if (ImGui::BeginCombo("##pfcombo", mPixelFilters[mGlobalUbo.pixel_filter_type].c_str(), ImGuiComboFlags_NoArrowButton)) {
				for (uint32_t i = 0; i < mPixelFilters.size(); i++) {
					const bool isSelected = (i == mGlobalUbo.pixel_filter_type);
					if (ImGui::Selectable(mPixelFilters[i].c_str(), isSelected)) mGlobalUbo.pixel_filter_type = i;
					if (isSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::DragFloat("##pfwidth", &mGlobalUbo.pixel_filter_width, 0.01f, 0.0f, 10.0f, "Width: %g px");
			if (mGlobalUbo.pixel_filter_type == 2) {
				static float alpha = 2;
				ImGui::DragFloat("##pfalpha", &alpha, 0.01f, 0.01f, 10, "Alpha: %g");
				mGlobalUbo.pixel_filter_extra.x = alpha;
			}
			if (mGlobalUbo.pixel_filter_type == 4) {
				static float B = 1.0f / 3.0f, C = 1.0f/3.0f;
				ImGui::DragFloat("##pfb", &B, 0.01f, 0.01f, 10.0f, "B: %g");
				ImGui::DragFloat("##pfc", &C, 0.01f, 0.01f, 10.0f, "C: %g");
				mGlobalUbo.pixel_filter_extra = glm::vec2(B, C);
			}

		}
		if (ImGui::CollapsingHeader("Film", 0)) {
			ImGui::DragFloat("##exposure_film", &mGlobalUbo.exposure_film, 0.01f, 0.0f, 10.0f, "Exposure: %g");
		}
		if (ImGui::CollapsingHeader("Tone Mapping", 0)) {
			if (ImGui::BeginCombo("##tmcombo", mToneMappings[mGlobalUbo.tone_mapping_type].c_str(), ImGuiComboFlags_NoArrowButton)) {
				for (uint32_t i = 0; i < mToneMappings.size(); i++) {
					const bool is_selected = (i == mGlobalUbo.tone_mapping_type);
					if (ImGui::Selectable(mToneMappings[i].c_str(), is_selected)) mGlobalUbo.tone_mapping_type = i;
					if (is_selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::DragFloat("##exposure_tm", &mGlobalUbo.exposure_tm, 0.01f, -10.0f, 10.0f, "Exposure: %g");
			ImGui::DragFloat("##gamma_tm", &mGlobalUbo.gamma_tm, 0.01f, 0.0f, 5.0f, "Gamma: %g");
		}
		ImGui::End();
	}
	if (ImGui::Begin("Statistics", nullptr, 0))
	{
		ImGui::Separator();
		ImGui::Text("Samples: %d", mGlobalUbo.accumulatedFrames);
		ImGui::End();
	}
}

#pragma once
#include <tamashii/engine/common/vars.hpp>
#include <tamashii/engine/render/render_backend_implementation.hpp>
#include <rvk/rvk.hpp>

#include <tamashii/renderer_vk/convenience/texture_to_gpu.hpp>
#include <tamashii/renderer_vk/convenience/geometry_to_gpu.hpp>
#include <tamashii/renderer_vk/convenience/light_to_gpu.hpp>
#include <tamashii/renderer_vk/convenience/material_to_gpu.hpp>

T_BEGIN_NAMESPACE
class raytracingDefault final : public RenderBackendImplementation {
public:
						raytracingDefault(rvk::LogicalDevice* aDevice, rvk::Swapchain* aSwapchain, const uint32_t aFrameCount,
							std::function<uint32_t()> aCurrentFrame,
							std::function<rvk::CommandBuffer* ()> aGetCurrentCmdBuffer,
							std::function<rvk::SingleTimeCommand()> aGetStcBuffer) :
							mDevice(aDevice), mSwapchain(aSwapchain), mFrameCount(aFrameCount),
							mCurrentFrame(std::move(aCurrentFrame)), mGetCurrentCmdBuffer(std::move(aGetCurrentCmdBuffer)),
							mGetStcBuffer(std::move(aGetStcBuffer)),
							mPrevFrame(0), mGpuTd(aDevice), mGpuBlas(aDevice), mGpuLd(aDevice), mGpuMd(aDevice)
						{
							mGlobalUbo.dither_strength = 1.0f;
							mGlobalUbo.pixelSamplesPerFrame = 1;
							mGlobalUbo.accumulatedFrames = 0;

							mGlobalUbo.fr_tmin = 0.2f;
							mGlobalUbo.fr_tmax = 10000.0f;
							mGlobalUbo.br_tmin = 0.001f;
							mGlobalUbo.br_tmax = 10000.0f;
							mGlobalUbo.sr_tmin = 0.001f;
							mGlobalUbo.sr_tmax_offset = -0.01f; // offset for shadow ray tmax

							mGlobalUbo.pixel_filter_type = 0;
							mGlobalUbo.pixel_filter_width = 1.0f;
							mGlobalUbo.pixel_filter_extra = glm::vec2(2.0f);

							mGlobalUbo.exposure_film = 1;

							mGlobalUbo.exposure_tm = 0;
							mGlobalUbo.gamma_tm = 1;
							mGlobalUbo.rrpt = 5;
							mEnvLight = glm::vec4(var::bg.getInt3(), 255.f) / 255.0f;

							mSampling = {"Lights", "BRDF", "Lights (MIS)"};
							mPixelFilters = {"Box", "Triangle", "Gaussian", "Blackman-Harris", "Mitchell-Netravali"};
							mToneMappings = {
								"None", "Reinhard", "Reinhard Ext", "Uncharted2", "Uchimura", "Lottes", "Filmic", "ACES"
							};
							mCullMode = { "None", "Front", "Back" };
						}
						~raytracingDefault() override = default;

	const char*			getName() override { return "Ray Tracing"; }

	void				windowSizeChanged(int aWidth, int aHeight) override;
	void				screenshot(const std::string& aFilename) override;

						// implementation preparation
	void				prepare(renderInfo_s* aRenderInfo) override;
	void				destroy() override;

						// scene
	void				sceneLoad(scene_s aScene) override;
	void				sceneUnload(scene_s aScene) override;

						// frame
    void				drawView(viewDef_s* aViewDef) override;
    void				drawUI(uiConf_s* aUiConf) override;

private:
	rvk::LogicalDevice*										mDevice;
	rvk::Swapchain*											mSwapchain;
	uint32_t												mFrameCount;
	std::function<uint32_t()>								mCurrentFrame;
	std::function<rvk::CommandBuffer* ()>					mGetCurrentCmdBuffer;
	std::function<rvk::SingleTimeCommand()>					mGetStcBuffer;
	uint32_t												mPrevFrame;
	// convenience classes for textures, geometry and as
	TextureDataVulkan										mGpuTd;
	GeometryDataBlasVulkan									mGpuBlas;
	LightDataVulkan											mGpuLd;
	std::vector<GeometryDataTlasVulkan>						mGpuTlas;	// one separate tlas for each image
	std::vector<SceneUpdates_s>								mUpdates;	// one for each swapchain images
	MaterialDataVulkan										mGpuMd;

	// data that is frame independent
	struct VkData {
		explicit VkData(rvk::LogicalDevice* aDevice) : rtshader(aDevice), rtpipeline(aDevice), rtImageAccumulate(aDevice),
			rtImageAccumulateCount(aDevice) {}
		rvk::RTShader										rtshader;
		rvk::RTPipeline										rtpipeline;

		rvk::Image											rtImageAccumulate;
		rvk::Image											rtImageAccumulateCount;
	};
	// data that is frame dependent
	struct VkFrameData {
		explicit VkFrameData(rvk::LogicalDevice* aDevice) : globalDescriptor(aDevice), globalUniformBuffer(aDevice),
				rtImage(aDevice), debugImage(aDevice) {}
		// descriptor
		rvk::Descriptor										globalDescriptor;
		// buffers
		rvk::Buffer											globalUniformBuffer;
		rvk::Image											rtImage;
		rvk::Image											debugImage;
	};
	std::optional<VkData>									mVkData;
	std::vector<VkFrameData>								mVkFrameData;

	// c++/glsl shared definitions
	// include it inside class in order to not have it visible in global space
	#include "../../../assets/shader/raytracing_default/defines.h"
	GlobalUbo_s												mGlobalUbo = {};

	float													mEnvLightIntensity = 1.0f;
	glm::vec3												mEnvLight = {};
	bool													mEnvMapShading = true;
	bool													mRecalculate = false;
	bool													mDebugImage = false;
	bool													mDebugOutput = false;

	uint32_t												mActiveCullMode = 0;
	std::vector<std::string>								mCullMode;
	std::vector<std::string>								mSampling;
	std::vector<std::string>								mPixelFilters;
	std::vector<std::string>								mToneMappings;
};
T_END_NAMESPACE
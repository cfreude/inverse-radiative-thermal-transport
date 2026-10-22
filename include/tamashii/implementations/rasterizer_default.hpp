#pragma once
#include <tamashii/engine/render/render_backend_implementation.hpp>
#include <rvk/rvk.hpp>

#include <tamashii/renderer_vk/convenience/texture_to_gpu.hpp>
#include <tamashii/renderer_vk/convenience/geometry_to_gpu.hpp>
#include <tamashii/renderer_vk/convenience/material_to_gpu.hpp>
#include <tamashii/renderer_vk/convenience/light_to_gpu.hpp>
T_BEGIN_NAMESPACE
class rasterizerDefault final : public RenderBackendImplementation {
public:
						rasterizerDefault(rvk::LogicalDevice* aDevice, rvk::Swapchain* aSwapchain, const uint32_t aFrameCount,
							std::function<uint32_t()> aCurrentFrame,
							std::function<rvk::CommandBuffer*()> aGetCurrentCmdBuffer,
							std::function<rvk::SingleTimeCommand()> aGetStcBuffer) :
							mDevice(aDevice), mSwapchain(aSwapchain), mFrameCount(aFrameCount),
							mCurrentFrame(std::move(aCurrentFrame)),
							mGetCurrentCmdBuffer(std::move(aGetCurrentCmdBuffer)),
							mGetStcBuffer(std::move(aGetStcBuffer)), mGpuTd(aDevice), mGpuGd(aDevice), mGpuMd(aDevice),
							mGpuLd(aDevice), mActiveCullMode(0), mActiveDisplayMode(0), mRgbOrAlpha(false)
						{
							mCullMode = { "None", "Front", "Back", "Front and Back" };
							mDisplayMode = { "Default", "Vertex Color", "Metallic", "Roughness", "Normal Map", "Occlusion", "Light Map"};
						}

						~rasterizerDefault() override = default;

	const char*			getName() override { return "Rasterizer"; }
						// callbacks
	void				windowSizeChanged(int aWidth, int aHeight) override;
	void				entityAdded(const Ref_s* aRef) override;
	void				entityRemoved(const Ref_s* aRef) override;
	bool				drawOnMesh(const DrawInfo_s* aDrawInfo) override;

						// implementation preparation
	void				prepare(renderInfo_s* aRenderInfo) override;
	void				destroy() override;

	void				prepareTBNVis();
	void				prepareBoundingBoxVis();
	void				prepareCircleVis();

						// scene
	void				sceneLoad(scene_s aScene) override;
	void				sceneUnload(scene_s aScene) override;

						// frame
    void				drawView(viewDef_s* aViewDef) override;
    void				drawUI(uiConf_s* aUiConf) override;

	void				drawTBNVis(const rvk::CommandBuffer* aCb, const viewDef_s* aViewDef);
	void				drawBoundingBoxVis(const rvk::CommandBuffer* aCb, const viewDef_s* aViewDef);
	void				drawCircleVis(const rvk::CommandBuffer* aCb, const viewDef_s* aViewDef);

private:
	rvk::LogicalDevice*										mDevice;
	rvk::Swapchain*											mSwapchain;
	uint32_t												mFrameCount;
	std::function<uint32_t()>								mCurrentFrame;
	std::function<rvk::CommandBuffer* ()>					mGetCurrentCmdBuffer;
	std::function<rvk::SingleTimeCommand()>					mGetStcBuffer;

	TextureDataVulkan										mGpuTd;
	GeometryDataVulkan										mGpuGd;
	MaterialDataVulkan										mGpuMd;
	LightDataVulkan											mGpuLd;

	// data that is frame independent
	struct VkData {
		explicit VkData(rvk::LogicalDevice* aDevice) : shader(aDevice),
			pipeline_cull_none(aDevice), pipeline_cull_front(aDevice), pipeline_cull_back(aDevice), pipeline_cull_both(aDevice),
			pipeline_wireframe_cull_none(aDevice), pipeline_wireframe_cull_front(aDevice), pipeline_wireframe_cull_back(aDevice),
			pipeline_wireframe_cull_both(aDevice), current_pipeline(nullptr),
			shader_tbn_vis(aDevice),pipeline_tbn_vis(aDevice),shader_bounding_box_vis(aDevice),pipeline_bounding_box_vis(aDevice),
			shader_circle_vis(aDevice), pipeline_circle_vis(aDevice), rp(aDevice) {}

		rvk::RShader										shader;
		rvk::RPipeline										pipeline_cull_none;
		rvk::RPipeline										pipeline_cull_front;
		rvk::RPipeline										pipeline_cull_back;
		rvk::RPipeline										pipeline_cull_both;
		rvk::RPipeline										pipeline_wireframe_cull_none;
		rvk::RPipeline										pipeline_wireframe_cull_front;
		rvk::RPipeline										pipeline_wireframe_cull_back;
		rvk::RPipeline										pipeline_wireframe_cull_both;

		rvk::RPipeline										*current_pipeline;

		rvk::RShader										shader_tbn_vis;
		rvk::RPipeline										pipeline_tbn_vis;
		rvk::RShader										shader_bounding_box_vis;
		rvk::RPipeline										pipeline_bounding_box_vis;
		rvk::RShader										shader_circle_vis;
		rvk::RPipeline										pipeline_circle_vis;

		rvk::Renderpass										rp;
	};
	// data that is frame dependent
	struct VkFrameData {
		explicit VkFrameData(rvk::LogicalDevice* aDevice) : mGlobalDescriptor(aDevice), mTbnVisDescriptor(aDevice),
			mGlobalUniformBuffer(aDevice), mColor(aDevice), mDepth(aDevice), mFramebuffer(aDevice) {}

		// descriptor
		rvk::Descriptor										mGlobalDescriptor;
		rvk::Descriptor										mTbnVisDescriptor;
		// buffers
		rvk::Buffer											mGlobalUniformBuffer;

		rvk::Image											mColor;
		rvk::Image											mDepth;
		rvk::Framebuffer									mFramebuffer;
	};
	std::optional<VkData>									mVkData;
	std::vector<VkFrameData>								mVkFrameData;
	bool													mShade = false;
	bool													mWireframe = false;
	bool													mUseLightMaps = false;
	bool													mMulVertexColors = false;
	float													mDitherStrength = 1.0f;
	bool													mShowWireframe = false;
	bool													mShowTbn = false;
	bool													mShowModelBb = false;
	bool													mShowMeshBb = false;
	float													mTbnScale = 0.01f;
	float													mTbnDepthBiasConstant = 0.0f;
	float													mTbnDepthBiasSlope = 0.0f;

	std::vector<std::string>								mCullMode;
	uint32_t												mActiveCullMode;
	std::vector<std::string>								mDisplayMode;
	uint32_t												mActiveDisplayMode;
	bool													mRgbOrAlpha;
};
T_END_NAMESPACE
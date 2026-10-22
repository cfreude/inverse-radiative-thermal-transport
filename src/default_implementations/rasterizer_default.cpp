#include <tamashii/implementations/rasterizer_default.hpp>
#include <imgui.h>
// engine imports
#include <tamashii/engine/common/common.hpp>
#include <tamashii/engine/common/vars.hpp>
#include <tamashii/engine/platform/filewatcher.hpp>
// c++/glsl shared definitions

#include "../../../assets/shader/rasterizer_default/defines.h"

T_USE_NAMESPACE
RVK_USE_NAMESPACE

constexpr uint32_t VK_GLOBAL_IMAGE_SIZE = 256;
constexpr uint32_t VK_MATERIAL_SIZE = (3 * 1024);
constexpr uint32_t VK_LIGHT_SIZE = 128;

constexpr VkFormat COLOR_FORMAT = VK_FORMAT_B8G8R8A8_UNORM;
constexpr VkFormat DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

void rasterizerDefault::windowSizeChanged(const int aWidth, const int aHeight) {
	SingleTimeCommand stc = mGetStcBuffer();
	stc.begin();
	for (uint32_t si_idx = 0; si_idx < mFrameCount; si_idx++) {
		VkFrameData& frameData = mVkFrameData[si_idx];
		frameData.mColor.destroy();
		frameData.mDepth.destroy();
		frameData.mFramebuffer.destroy();

		frameData.mColor.createImage2D(aWidth, aHeight, COLOR_FORMAT, rvk::Image::Use::DOWNLOAD | rvk::Image::Use::COLOR_ATTACHMENT);
		frameData.mDepth.createImage2D(aWidth, aHeight, DEPTH_FORMAT, rvk::Image::Use::DOWNLOAD | rvk::Image::Use::DEPTH_STENCIL_ATTACHMENT);
		frameData.mColor.CMD_TransitionImage(stc.buffer(),VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		frameData.mDepth.CMD_TransitionImage(stc.buffer(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		frameData.mFramebuffer.addAttachment(&frameData.mColor);
		frameData.mFramebuffer.addAttachment(&frameData.mDepth);
		frameData.mFramebuffer.setRenderpass(&mVkData->rp);
		frameData.mFramebuffer.finish();
	}
	stc.end();
}

void rasterizerDefault::entityAdded(const Ref_s* aRef)
{
}

void rasterizerDefault::entityRemoved(const Ref_s* aRef)
{
}

bool rasterizerDefault::drawOnMesh(const DrawInfo_s* aDrawInfo)
{
	return false;
}

// implementation preparation
void rasterizerDefault::prepare(renderInfo_s* aRenderInfo) {
	SingleTimeCommand stc = mGetStcBuffer();

	mGpuTd.prepare(rvk::Shader::Stage::FRAGMENT, VK_GLOBAL_IMAGE_SIZE);
	mGpuMd.prepare(rvk::Buffer::Use::STORAGE, VK_MATERIAL_SIZE, true);
	mGpuLd.prepare(rvk::Buffer::Use::STORAGE, VK_LIGHT_SIZE);

	int count = 0;
	mVkData.emplace(mDevice);
	mVkFrameData.resize(mFrameCount, VkFrameData(mDevice));

	rvk::AttachmentDescription ca(COLOR_FORMAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	ca.setAttachmentOps(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
	rvk::AttachmentDescription da(DEPTH_FORMAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	da.setAttachmentOps(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
	mVkData->rp.addAttachment(ca);
	mVkData->rp.addAttachment(da);
	mVkData->rp.addSubpass(VK_PIPELINE_BIND_POINT_GRAPHICS, 2);
	mVkData->rp.setColorAttachment(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	mVkData->rp.setDepthStencilAttachment(0, 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	mVkData->rp.finish();
	RPipeline::global_render_state.renderpass = mVkData->rp.getHandle();
	RPipeline::global_render_state.scissor.extent.width = aRenderInfo->target_size.x;
	RPipeline::global_render_state.scissor.extent.height = aRenderInfo->target_size.y;
	RPipeline::global_render_state.viewport.height = static_cast<float>(aRenderInfo->target_size.y);
	RPipeline::global_render_state.viewport.width = static_cast<float>(aRenderInfo->target_size.x);

	// data for individual frames
	for (uint32_t frameIndex = 0; frameIndex < mFrameCount; frameIndex++) {
		VkFrameData& frameData = mVkFrameData[frameIndex];

		frameData.mColor.createImage2D(aRenderInfo->target_size.x, aRenderInfo->target_size.y, COLOR_FORMAT, rvk::Image::Use::DOWNLOAD | rvk::Image::Use::COLOR_ATTACHMENT);
		frameData.mDepth.createImage2D(aRenderInfo->target_size.x, aRenderInfo->target_size.y, DEPTH_FORMAT, rvk::Image::Use::DOWNLOAD | rvk::Image::Use::DEPTH_STENCIL_ATTACHMENT);
		stc.begin();
		frameData.mColor.CMD_TransitionImage(stc.buffer(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		frameData.mDepth.CMD_TransitionImage(stc.buffer(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		stc.end();
		frameData.mFramebuffer.addAttachment(&frameData.mColor);
		frameData.mFramebuffer.addAttachment(&frameData.mDepth);
		frameData.mFramebuffer.setRenderpass(&mVkData->rp);
		frameData.mFramebuffer.finish();

		//frameData.framebuffer.setRenderpass(vk.swapchain.renderpassClear);

		// global uniform buffer
		frameData.mGlobalUniformBuffer.create(rvk::Buffer::Use::UNIFORM, sizeof(GlobalUbo), rvk::Buffer::Location::HOST_COHERENT);
		frameData.mGlobalUniformBuffer.mapBuffer();

		// descriptors
		// global descriptor
		frameData.mGlobalDescriptor.addUniformBuffer(GLOBAL_DESC_UBO_BINDING, rvk::Shader::Stage::VERTEX | rvk::Shader::Stage::GEOMETRY | rvk::Shader::Stage::FRAGMENT);
		frameData.mGlobalDescriptor.addStorageBuffer(GLOBAL_DESC_MATERIAL_BUFFER_BINDING, rvk::Shader::Stage::VERTEX | rvk::Shader::Stage::FRAGMENT);
		frameData.mGlobalDescriptor.addStorageBuffer(GLOBAL_DESC_LIGHT_BUFFER_BINDING, rvk::Shader::Stage::VERTEX | rvk::Shader::Stage::FRAGMENT);

		frameData.mGlobalDescriptor.setBuffer(GLOBAL_DESC_UBO_BINDING, &frameData.mGlobalUniformBuffer);
		frameData.mGlobalDescriptor.setBuffer(GLOBAL_DESC_MATERIAL_BUFFER_BINDING, mGpuMd.getMaterialBuffer());
		frameData.mGlobalDescriptor.setBuffer(GLOBAL_DESC_LIGHT_BUFFER_BINDING, mGpuLd.getLightBuffer());

		frameData.mGlobalDescriptor.finish(true);
	}

	// other stuff
	mVkData->shader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::VERTEX, "assets/shader/rasterizer_default/shade.vert", { "GLSL" });
	mVkData->shader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::GEOMETRY, "assets/shader/rasterizer_default/shade.geom", { "GLSL" });
	mVkData->shader.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::FRAGMENT, "assets/shader/rasterizer_default/shade.frag", { "GLSL" });
	mVkData->shader.finish();
	tamashii::FileWatcher::getInstance().watchFile("assets/shader/rasterizer_default/shade.vert", [this]() { mVkData->shader.reloadShader("assets/shader/rasterizer_default/shade.vert"); });
	tamashii::FileWatcher::getInstance().watchFile("assets/shader/rasterizer_default/shade.frag", [this]() { mVkData->shader.reloadShader("assets/shader/rasterizer_default/shade.frag"); });

	mVkData->pipeline_cull_none.setShader(&mVkData->shader);
	mVkData->pipeline_cull_none.addPushConstant(rvk::Shader::Stage::VERTEX | rvk::Shader::Stage::FRAGMENT, 0, 17 * sizeof(float));
	mVkData->pipeline_cull_none.addDescriptorSet({ mGpuTd.getDescriptor(), &mVkFrameData[0].mGlobalDescriptor });

	mVkData->pipeline_cull_none.addBindingDescription(0, sizeof(vertex_s), VK_VERTEX_INPUT_RATE_VERTEX);
	mVkData->pipeline_cull_none.addAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex_s, position));
	mVkData->pipeline_cull_none.addAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex_s, normal));
	mVkData->pipeline_cull_none.addAttributeDescription(0, 2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex_s, tangent));
	mVkData->pipeline_cull_none.addAttributeDescription(0, 3, VK_FORMAT_R32G32_SFLOAT, offsetof(vertex_s, texture_coordinates_0));
	mVkData->pipeline_cull_none.addAttributeDescription(0, 4, VK_FORMAT_R32G32_SFLOAT, offsetof(vertex_s, texture_coordinates_1));
	mVkData->pipeline_cull_none.addAttributeDescription(0, 5, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex_s, color_0));
	mVkData->pipeline_cull_none.finish();
	rvk::RPipeline::pushRenderState();
	rvk::RPipeline::global_render_state.cullMode = VK_CULL_MODE_FRONT_BIT;
	mVkData->pipeline_cull_front = mVkData->pipeline_cull_none;
	mVkData->pipeline_cull_front.finish();
	rvk::RPipeline::global_render_state.cullMode = VK_CULL_MODE_BACK_BIT;
	mVkData->pipeline_cull_back = mVkData->pipeline_cull_none;
	mVkData->pipeline_cull_back.finish();
	rvk::RPipeline::global_render_state.cullMode = VK_CULL_MODE_FRONT_AND_BACK;
	mVkData->pipeline_cull_both = mVkData->pipeline_cull_none;
	mVkData->pipeline_cull_both.finish();
	rvk::RPipeline::popRenderState();

	// wireframe
	rvk::RPipeline::pushRenderState();
	rvk::RPipeline::global_render_state.polygonMode = VK_POLYGON_MODE_LINE;
	mVkData->pipeline_wireframe_cull_none = mVkData->pipeline_cull_none;
	mVkData->pipeline_wireframe_cull_none.finish();
	rvk::RPipeline::global_render_state.cullMode = VK_CULL_MODE_FRONT_BIT;
	mVkData->pipeline_wireframe_cull_front = mVkData->pipeline_wireframe_cull_none;
	mVkData->pipeline_wireframe_cull_front.finish();
	rvk::RPipeline::global_render_state.cullMode = VK_CULL_MODE_BACK_BIT;
	mVkData->pipeline_wireframe_cull_back = mVkData->pipeline_wireframe_cull_none;
	mVkData->pipeline_wireframe_cull_back.finish();
	rvk::RPipeline::global_render_state.cullMode = VK_CULL_MODE_FRONT_AND_BACK;
	mVkData->pipeline_wireframe_cull_both = mVkData->pipeline_wireframe_cull_none;
	mVkData->pipeline_wireframe_cull_both.finish();
	rvk::RPipeline::popRenderState();

	prepareTBNVis();
	prepareBoundingBoxVis();
	//prepareCircleVis();

	RPipeline::global_render_state.renderpass = nullptr;
}
void rasterizerDefault::destroy() {
	mGpuTd.destroy();
	mGpuMd.destroy();
	mGpuLd.destroy();
	tamashii::FileWatcher::getInstance().removeFile("assets/shader/rasterizer_default/shade.vert");
	tamashii::FileWatcher::getInstance().removeFile("assets/shader/rasterizer_default/shade.frag");

	mVkData.reset();
	mVkFrameData.clear();
}

void rasterizerDefault::prepareTBNVis()
{
	// data for individual frames
	for (uint32_t si_idx = 0; si_idx < mFrameCount; si_idx++) {
		VkFrameData& frameData = mVkFrameData[si_idx];
		// tbn
		frameData.mTbnVisDescriptor.addUniformBuffer(0, rvk::Shader::Stage::GEOMETRY);
		frameData.mTbnVisDescriptor.setBuffer(0, &frameData.mGlobalUniformBuffer);
		frameData.mTbnVisDescriptor.finish(true);
	}
	mVkData->shader_tbn_vis.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::VERTEX, "assets/shader/rasterizer_default/tbn_vis.vert", { "GLSL" });
	mVkData->shader_tbn_vis.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::GEOMETRY, "assets/shader/rasterizer_default/tbn_vis.geom", { "GLSL" });
	mVkData->shader_tbn_vis.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::FRAGMENT, "assets/shader/rasterizer_default/tbn_vis.frag", { "GLSL" });
	mVkData->shader_tbn_vis.finish();

	mVkData->pipeline_tbn_vis.setShader(&mVkData->shader_tbn_vis);
	mVkData->pipeline_tbn_vis.addPushConstant(rvk::Shader::Stage::VERTEX, 0, sizeof(glm::mat4) + sizeof(float));
	mVkData->pipeline_tbn_vis.addDescriptorSet({ &mVkFrameData[0].mTbnVisDescriptor });
	mVkData->pipeline_tbn_vis.addBindingDescription(0, sizeof(tamashii::vertex_s), VK_VERTEX_INPUT_RATE_VERTEX);
	mVkData->pipeline_tbn_vis.addAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(tamashii::vertex_s, position));
	mVkData->pipeline_tbn_vis.addAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(tamashii::vertex_s, normal));
	mVkData->pipeline_tbn_vis.addAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(tamashii::vertex_s, tangent));
	rvk::RPipeline::pushRenderState();
	rvk::RPipeline::global_render_state.dynamicStates.depth_bias = true;
	rvk::RPipeline::global_render_state.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	rvk::RPipeline::global_render_state.polygonMode = VK_POLYGON_MODE_FILL;
	rvk::RPipeline::global_render_state.cullMode = VK_CULL_MODE_NONE;
	mVkData->pipeline_tbn_vis.finish();
	rvk::RPipeline::popRenderState();
}

void rasterizerDefault::prepareBoundingBoxVis()
{
	mVkData->shader_bounding_box_vis.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::VERTEX, "assets/shader/rasterizer_default/bounding_box.vert", { "GLSL" });
	mVkData->shader_bounding_box_vis.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::GEOMETRY, "assets/shader/rasterizer_default/bounding_box.geom", { "GLSL" });
	mVkData->shader_bounding_box_vis.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::FRAGMENT, "assets/shader/rasterizer_default/bounding_box.frag", { "GLSL" });
	mVkData->shader_bounding_box_vis.finish();

	mVkData->pipeline_bounding_box_vis.setShader(&mVkData->shader_bounding_box_vis);
	mVkData->pipeline_bounding_box_vis.addPushConstant(rvk::Shader::Stage::VERTEX, 0, sizeof(glm::mat4) + sizeof(glm::vec4) + sizeof(glm::vec4));
	mVkData->pipeline_bounding_box_vis.addDescriptorSet({ &mVkFrameData[0].mTbnVisDescriptor });
	rvk::RPipeline::pushRenderState();
	rvk::RPipeline::global_render_state.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	rvk::RPipeline::global_render_state.polygonMode = VK_POLYGON_MODE_LINE;
	rvk::RPipeline::global_render_state.cullMode = VK_CULL_MODE_NONE;
	mVkData->pipeline_bounding_box_vis.finish();
	rvk::RPipeline::popRenderState();
}

void rasterizerDefault::prepareCircleVis()
{
	mVkData->shader_circle_vis.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::VERTEX, "assets/shader/rasterizer_default/circle.vert", { "GLSL" });
	mVkData->shader_circle_vis.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::GEOMETRY, "assets/shader/rasterizer_default/circle.geom", { "GLSL" });
	mVkData->shader_circle_vis.addStage(rvk::Shader::Source::GLSL, rvk::Shader::Stage::FRAGMENT, "assets/shader/rasterizer_default/circle.frag", { "GLSL" });
	mVkData->shader_circle_vis.finish();

	mVkData->pipeline_circle_vis.setShader(&mVkData->shader_circle_vis);
	mVkData->pipeline_circle_vis.addPushConstant(rvk::Shader::Stage::VERTEX, 0, sizeof(glm::vec4));
	mVkData->pipeline_circle_vis.addPushConstant(rvk::Shader::Stage::FRAGMENT, sizeof(glm::vec4), sizeof(glm::vec3));
	mVkData->pipeline_circle_vis.addPushConstant(rvk::Shader::Stage::GEOMETRY, sizeof(glm::vec4) + sizeof(glm::vec3), sizeof(float));
	mVkData->pipeline_circle_vis.addDescriptorSet({ &mVkFrameData[0].mTbnVisDescriptor });
	rvk::RPipeline::pushRenderState();
	rvk::RPipeline::global_render_state.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	rvk::RPipeline::global_render_state.polygonMode = VK_POLYGON_MODE_FILL;
	rvk::RPipeline::global_render_state.cullMode = VK_CULL_MODE_NONE;
	mVkData->pipeline_circle_vis.finish();
	rvk::RPipeline::popRenderState();
}

// scene
void rasterizerDefault::sceneLoad(scene_s aScene) {
	const GeometryDataVulkan::SceneInfo_s sinfo = GeometryDataVulkan::getSceneGeometryInfo(aScene);
	if (!sinfo.mMeshCount) mGpuGd.prepare();
	else mGpuGd.prepare(sinfo.mIndexCount, sinfo.mVertexCount);

	SingleTimeCommand stc = mGetStcBuffer();
	mGpuTd.loadScene(&stc, aScene);
	mGpuGd.loadScene(&stc, aScene);
	mGpuMd.loadScene(&stc, aScene, &mGpuTd);
	mGpuLd.loadScene(&stc, aScene, &mGpuTd);

	for (uint32_t idx = 0; idx < mFrameCount; idx++) {
		if (mGpuMd.bufferChanged(false)) mVkFrameData[idx].mGlobalDescriptor.setBuffer(GLOBAL_DESC_MATERIAL_BUFFER_BINDING, mGpuMd.getMaterialBuffer());
		mVkFrameData[idx].mGlobalDescriptor.update();
	}

	mShade = !aScene.refLights.empty();
}
void rasterizerDefault::sceneUnload(scene_s aScene) {
	mGpuTd.unloadScene();
	mGpuGd.unloadScene();
	mGpuMd.unloadScene();
	mGpuLd.unloadScene();

	mGpuGd.destroy();
}

// frame
void rasterizerDefault::drawView(viewDef_s* aViewDef) {
	CommandBuffer* cb = mGetCurrentCmdBuffer();
	const glm::vec3 cc = glm::vec3(var::bg.getInt3()) / 255.0f;
	//rvk::render::beginRenderClearSC(cb, { cc.x, cc.y, cc.z, 1.0f });
	/*mSwapchain->CMD_BeginRenderClear(cb, { {cc.x, cc.y, cc.z, 1.0f},  {0.0f, 0.0f} });
	mSwapchain->CMD_EndRender(cb);
	return;*/
	mVkFrameData[mCurrentFrame()].mFramebuffer.CMD_BeginRenderClear(cb, { {cc.x, cc.y, cc.z, 1.0f},  {0.0f, 0.0f} });

	SingleTimeCommand stc = mGetStcBuffer();
	if (aViewDef->updates.mImages || aViewDef->updates.mTextures) {
		mDevice->waitIdle();
		mGpuTd.update(&stc, aViewDef->scene);
	}
	if (aViewDef->updates.mMaterials) mGpuMd.update(&stc, aViewDef->scene, &mGpuTd);
	if (aViewDef->updates.mLights) mGpuLd.update(&stc, aViewDef->scene, &mGpuTd);
	if (aViewDef->updates.mModelGeometries) mGpuGd.update(&stc, aViewDef->scene);

	// ubo
	GlobalUbo ubo;
	ubo.viewMat = aViewDef->view_matrix;
	ubo.projMat = aViewDef->projection_matrix;
	ubo.inverseViewMat = aViewDef->inv_view_matrix;
	ubo.inverseProjMat = aViewDef->inv_projection_matrix;
	ubo.viewPos = glm::vec4(aViewDef->view_pos, 1);
	ubo.viewDir = glm::vec4(aViewDef->view_dir, 0);
	ubo.size[0] = aViewDef->target_size.x; ubo.size[1] = aViewDef->target_size.y;
	ubo.shade = mShade;
	ubo.dither_strength = mDitherStrength;
	ubo.light_count = mGpuLd.getLightCount();
	ubo.display_mode = mActiveDisplayMode;
	ubo.rgb_or_alpha = mRgbOrAlpha;
	ubo.normal_color = glm::vec4(0, 1, 0, 1);
	ubo.tangent_color = glm::vec4(1, 0, 0, 1);
	ubo.bitangent_color = glm::vec4(0, 0, 1, 1);
	ubo.wireframe = mShowWireframe;
	ubo.useLightMaps = mUseLightMaps;
	ubo.mulVertexColors = mMulVertexColors;
	mVkFrameData[mCurrentFrame()].mGlobalUniformBuffer.STC_UploadData(&stc, &ubo, sizeof(GlobalUbo));
	Common::getInstance().intersectionSettings().mCullMode = static_cast<CullMode_e>(mActiveCullMode);

	// render surfaces
	if (mWireframe) {
		if (mActiveCullMode == 0) mVkData->current_pipeline = &mVkData->pipeline_wireframe_cull_none;
		if (mActiveCullMode == 1) mVkData->current_pipeline = &mVkData->pipeline_wireframe_cull_front;
		if (mActiveCullMode == 2) mVkData->current_pipeline = &mVkData->pipeline_wireframe_cull_back;
		if (mActiveCullMode == 3) mVkData->current_pipeline = &mVkData->pipeline_wireframe_cull_both;
	}
	else
	{
		if (mActiveCullMode == 0) mVkData->current_pipeline = &mVkData->pipeline_cull_none;
		if (mActiveCullMode == 1) mVkData->current_pipeline = &mVkData->pipeline_cull_front;
		if (mActiveCullMode == 2) mVkData->current_pipeline = &mVkData->pipeline_cull_back;
		if (mActiveCullMode == 3) mVkData->current_pipeline = &mVkData->pipeline_cull_both;
	}

	mVkData->current_pipeline->CMD_SetViewport(cb);
	mVkData->current_pipeline->CMD_SetScissor(cb);
	mVkData->current_pipeline->CMD_BindDescriptorSets(cb, { mGpuTd.getDescriptor(), &mVkFrameData[mCurrentFrame()].mGlobalDescriptor });
	mVkData->current_pipeline->CMD_BindPipeline(cb);
	mGpuGd.getVertexBuffer()->CMD_BindVertexBuffer(cb, 0, 0);
	mGpuGd.getIndexBuffer()->CMD_BindIndexBuffer(cb, VK_INDEX_TYPE_UINT32, 0);

	
	for (const drawSurf_s& ds : aViewDef->surfaces) {
		int material_index = mGpuMd.getIndex(ds.refMesh->mesh->getMaterial());
		mVkData->current_pipeline->CMD_SetPushConstant(cb, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16 * sizeof(float), &ds.model_matrix[0][0]);
		mVkData->current_pipeline->CMD_SetPushConstant(cb, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 16 * sizeof(float), 1 * sizeof(int), &material_index);

		if (ds.refMesh->mesh->hasIndices()) mVkData->current_pipeline->CMD_DrawIndexed(cb, ds.refMesh->mesh->getIndexCount(),
			mGpuGd.getOffset(ds.refMesh->mesh).mIndexOffset, mGpuGd.getOffset(ds.refMesh->mesh).mVertexOffset);
		else mVkData->current_pipeline->CMD_Draw(cb, ds.refMesh->mesh->getVertexCount(), mGpuGd.getOffset(ds.refMesh->mesh).mVertexOffset);
	}
	// vis
	if (mShowTbn) drawTBNVis(cb, aViewDef);
	if (mShowMeshBb || mShowModelBb) drawBoundingBoxVis(cb, aViewDef);
	//drawCircleVis(cb, vd);
	//rvk::render::endRender(cb);
	mVkFrameData[mCurrentFrame()].mFramebuffer.CMD_EndRender(cb);


	if(!aViewDef->headless) rvk::swapchain::CMD_BlitImageToCurrentSwapchainImage(cb, mSwapchain, &mVkFrameData[mCurrentFrame()].mColor, VK_FILTER_LINEAR);
}
void rasterizerDefault::drawUI(uiConf_s* aUiConf) {
	if (ImGui::Begin("Settings", nullptr, 0))
	{
		ImGui::Separator();
		ImGui::Text("Cull Mode:");
		if (ImGui::BeginCombo("##cmcombo", mCullMode[mActiveCullMode].c_str(), ImGuiComboFlags_NoArrowButton)) {
			for (uint32_t i = 0; i < mCullMode.size(); i++) {
				const bool isSelected = (i == mActiveCullMode);
				if (ImGui::Selectable(mCullMode[i].c_str(), isSelected)) mActiveCullMode = i;
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::Text("Display Mode:");
		if (ImGui::BeginCombo("##dmcombo", mDisplayMode[mActiveDisplayMode].c_str(), ImGuiComboFlags_NoArrowButton)) {
			for (uint32_t i = 0; i < mDisplayMode.size(); i++) {
				const bool isSelected = (i == mActiveDisplayMode);
				if (ImGui::Selectable(mDisplayMode[i].c_str(), isSelected)) mActiveDisplayMode = i;
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if(mActiveDisplayMode == 1)
		{
			ImGui::Checkbox("Show Alpha only", &mRgbOrAlpha);
		}

		if (mActiveDisplayMode == 0) {
			ImGui::Checkbox("Shade", &mShade);
			ImGui::Checkbox("Lines", &mWireframe);
			ImGui::Checkbox("Use Lightmaps", &mUseLightMaps);
		}
		ImGui::Checkbox("Multiply Vertex Color", &mMulVertexColors);
		if (mActiveDisplayMode == 0) {
			ImGui::DragFloat("##dither", &mDitherStrength, 0.01f, 0, 1.0f, "Dither: %.3f");
		}

		if (ImGui::CollapsingHeader("Debug", 0)) {
			ImGui::Checkbox("Wireframe Overlay", &mShowWireframe);
			ImGui::Checkbox("Show Model Bounding Box", &mShowModelBb);
			ImGui::Checkbox("Show Mesh Bounding Box", &mShowMeshBb);
			ImGui::Checkbox("Show TBN", &mShowTbn);
			if (mShowTbn) ImGui::DragFloat("##tbnscale", &mTbnScale, 0.001f, 0.001f, 0.2f, "Scale: %.3f");
			//if (show_tbn) ImGui::SliderFloat("##tbndbconst", &tbn_depth_bias_constant, -100.0f, 100.0f, "Scale const: %.3f");
			//if (show_tbn) ImGui::SliderFloat("##tbndbslope", &tbn_depth_bias_slope, -100.0f, 100.0f, "Scale slope: %.3f");
		}
		ImGui::End();
	}
}

void rasterizerDefault::drawTBNVis(const rvk::CommandBuffer* aCb, const viewDef_s* aViewDef)
{
	mVkData->pipeline_tbn_vis.CMD_SetDepthBias(aCb, mTbnDepthBiasConstant, mTbnDepthBiasSlope);
	mVkData->pipeline_tbn_vis.CMD_BindDescriptorSets(aCb, { &mVkFrameData[mCurrentFrame()].mTbnVisDescriptor });
	mVkData->pipeline_tbn_vis.CMD_BindPipeline(aCb);
	for (const drawSurf_s& ds : aViewDef->surfaces) {
		mVkData->pipeline_tbn_vis.CMD_SetPushConstant(aCb, VK_SHADER_STAGE_VERTEX_BIT, 0, 16 * sizeof(float), &ds.model_matrix[0][0]);
		mVkData->pipeline_tbn_vis.CMD_SetPushConstant(aCb, VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), sizeof(float), &mTbnScale);
		mVkData->pipeline_tbn_vis.CMD_Draw(aCb, ds.refMesh->mesh->getVertexCount(), mGpuGd.getOffset(ds.refMesh->mesh).mVertexOffset);
	}
}

void rasterizerDefault::drawBoundingBoxVis(const rvk::CommandBuffer* aCb, const viewDef_s* aViewDef)
{
	mVkData->pipeline_bounding_box_vis.CMD_BindDescriptorSets(aCb, { &mVkFrameData[mCurrentFrame()].mTbnVisDescriptor });
	mVkData->pipeline_bounding_box_vis.CMD_BindPipeline(aCb);
	if (mShowMeshBb) {
		for (const drawSurf_s& rm : aViewDef->surfaces) {
			const aabb_s aabb = rm.refMesh->mesh->getAABB();
			mVkData->pipeline_bounding_box_vis.CMD_SetPushConstant(aCb, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &rm.model_matrix[0][0]);
			mVkData->pipeline_bounding_box_vis.CMD_SetPushConstant(aCb, VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4) + sizeof(glm::vec4), sizeof(glm::vec3), &aabb.mMin);
			mVkData->pipeline_bounding_box_vis.CMD_SetPushConstant(aCb, VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), sizeof(glm::vec3), &aabb.mMax);
			mVkData->pipeline_bounding_box_vis.CMD_Draw(aCb, 1, 0);
		}
	}
	if (mShowModelBb) {
		for (const RefModel_s* rm : aViewDef->ref_models) {
			const aabb_s aabb = rm->model->getAABB();
			mVkData->pipeline_bounding_box_vis.CMD_SetPushConstant(aCb, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &rm->model_matrix[0][0]);
			mVkData->pipeline_bounding_box_vis.CMD_SetPushConstant(aCb, VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4) + sizeof(glm::vec4), sizeof(glm::vec3), &aabb.mMin);
			mVkData->pipeline_bounding_box_vis.CMD_SetPushConstant(aCb, VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), sizeof(glm::vec3), &aabb.mMax);
			mVkData->pipeline_bounding_box_vis.CMD_Draw(aCb, 1, 0);
		}
	}
}

void rasterizerDefault::drawCircleVis(const rvk::CommandBuffer* aCb, const viewDef_s* aViewDef)
{
	mVkData->pipeline_circle_vis.CMD_BindDescriptorSets(aCb, { &mVkFrameData[mCurrentFrame()].mTbnVisDescriptor });
	mVkData->pipeline_circle_vis.CMD_BindPipeline(aCb);

	for (const RefLight_s* l : aViewDef->lights) {
		float radius = 0.1f;
		glm::vec3 pos = l->position;
		glm::vec3 color = glm::vec3(220.0f / 255.0f, 220.0f / 255.0f, 220.0f / 255.0f);
		//if(l == vd->) color = glm::vec3(225.0f / 255.0f, 156.0f / 255.0f, 99.0f / 255.0f);
		mVkData->pipeline_circle_vis.CMD_SetPushConstant(aCb, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec3), &pos[0]);
		mVkData->pipeline_circle_vis.CMD_SetPushConstant(aCb, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::vec4), sizeof(glm::vec3), &color[0]);
		mVkData->pipeline_circle_vis.CMD_SetPushConstant(aCb, VK_SHADER_STAGE_GEOMETRY_BIT, sizeof(glm::vec4) + sizeof(glm::vec3), sizeof(float), &radius);
		mVkData->pipeline_circle_vis.CMD_Draw(aCb, 1, 0);
	}
		
}

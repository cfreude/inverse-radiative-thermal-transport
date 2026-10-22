#include "parts/rvk_private.hpp"
#include <rvk/parts/command_buffer.hpp>
RVK_USE_NAMESPACE
class Swapchain;
class CommandBuffer;
using namespace rvk::memory;


//static float vFullscreenQuad[24] = { -1.0f, -1.0f, 1.0f, 0.0f,
//											 1.0f, -1.0f, 1.0f, 0.0f,
//											-1.0f,  1.0f, 1.0f, 0.0f,
//											 1.0f,  1.0f, 1.0f, 0.0f };
//static uint32_t idxFullscreenQuad[6] = { 0, 1, 2, 2, 1, 3 };
//static float uvFullscreenQuad[12] = { 0.0f, 0.0f,
//									1.0f, 0.0f,
//									0.0f,  1.0f,
//									1.0f,  1.0f};

/*
** CLEAR ATTACHMENT
*/
//void VK_ClearAttachments(qboolean clear_depth, qboolean clear_stencil, qboolean clear_color, vec4_t color) {
//    
//
//    if (!clear_depth && !clear_stencil && !clear_color)
//        return;
//    
//    if (clear_depth) clear_depth = 1;
//    if (clear_stencil) clear_stencil = 1;
//    if (clear_color) clear_color = 1;
//
//    // backup state
//    VkViewport vpSave = { 0 };
//    VkRect2D sSave = { 0 };
//    Com_Memcpy(&vpSave, &vk_d.viewport, sizeof(vk_d.viewport));
//    Com_Memcpy(&sSave, &vk_d.scissor, sizeof(vk_d.scissor));
//
//    memset(&vk_d.viewport, 0, sizeof(vk_d.viewport));
//    memset(&vk_d.scissor, 0, sizeof(vk_d.scissor));
//    vk_d.viewport.x = 0;
//    vk_d.viewport.y = 0;
//    vk_d.viewport.height = glConfig.vidHeight;
//    vk_d.viewport.width = glConfig.vidWidth;
//    vk_d.viewport.minDepth = 0.0f;
//    vk_d.viewport.maxDepth = 1.0f;
//    vk_d.scissor.offset.x = 0;
//    vk_d.scissor.offset.y = 0;
//    vk_d.scissor.extent.height = glConfig.vidHeight;
//    vk_d.scissor.extent.width = glConfig.vidWidth;
//
//    VK_UploadBufferDataOffset(&vk_d.indexbuffer, vk_d.offsetIdx * sizeof(uint32_t), sizeof(idxFullscreenQuad)/sizeof(idxFullscreenQuad[0]) * sizeof(uint32_t), (void*) &idxFullscreenQuad[0]);
//    VK_UploadBufferDataOffset(&vk_d.vertexbuffer, vk_d.offset * sizeof(vec4_t), sizeof(vFullscreenQuad)/sizeof(vFullscreenQuad[0]) * sizeof(vec4_t), (void*)&vFullscreenQuad[0]);
//    
//    vkpipeline_t p = { 0 };
//    VK_GetAttachmentClearPipelines(&p, clear_color, clear_depth, clear_stencil);
//
//    VK_SetPushConstant(&p, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(vec4_t), &color);
//	VK_BindPipeline(&p);
//    VK_DrawIndexed(&vk_d.indexbuffer, sizeof(idxFullscreenQuad)/sizeof(idxFullscreenQuad[0]), vk_d.offsetIdx, vk_d.offset);
//
//    vk_d.offsetIdx += sizeof(idxFullscreenQuad)/sizeof(idxFullscreenQuad[0]);
//    vk_d.offset += sizeof(vFullscreenQuad)/sizeof(vFullscreenQuad[0]);
//    
//    // restore state
//    Com_Memcpy(&vk_d.viewport, &vpSave, sizeof(vk_d.viewport));
//    Com_Memcpy(&vk_d.scissor, &sSave, sizeof(vk_d.scissor));
//}

/*
** FULLSCREEN RECT
*/
//void VK_DrawFullscreenRect(vkimage_t *image) {
//	if (vk_d.fullscreenRectPipeline.handle == VK_NULL_HANDLE) {
//	
//		vkshader_t s = { 0 };
//		VK_FullscreenRectShader(&s);
//		
//		vk_d.state.colorBlend.blendEnable = VK_TRUE;
//		vk_d.state.colorBlend.colorBlendOp = VK_BLEND_OP_ADD;
//		vk_d.state.colorBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
//		vk_d.state.colorBlend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
//		vk_d.state.colorBlend.alphaBlendOp = VK_BLEND_OP_ADD;
//		vk_d.state.colorBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
//		vk_d.state.colorBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
//		vk_d.state.dsBlend.depthTestEnable = VK_FALSE;
//
//		VK_SetDescriptorSet(&vk_d.fullscreenRectPipeline, &vk_d.images[0].descriptor_set);
//		VK_SetShader(&vk_d.fullscreenRectPipeline, &s);
//		VK_AddBindingDescription(&vk_d.fullscreenRectPipeline, 0, sizeof(vec4_t), VK_VERTEX_INPUT_RATE_VERTEX);
//		VK_AddBindingDescription(&vk_d.fullscreenRectPipeline, 2, sizeof(vec2_t), VK_VERTEX_INPUT_RATE_VERTEX);
//		VK_AddAttributeDescription(&vk_d.fullscreenRectPipeline, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0 * sizeof(float));
//		VK_AddAttributeDescription(&vk_d.fullscreenRectPipeline, 2, 2, VK_FORMAT_R32G32_SFLOAT, 0 * sizeof(float));
//		
//		VK_FinishPipeline(&vk_d.fullscreenRectPipeline);
//
//	}
//
//	// backup state
//	VkViewport vpSave = { 0 };
//	VkRect2D sSave = { 0 };
//	Com_Memcpy(&vpSave, &vk_d.viewport, sizeof(vk_d.viewport));
//	Com_Memcpy(&sSave, &vk_d.scissor, sizeof(vk_d.scissor));
//
//	VK_UploadBufferDataOffset(&vk_d.indexbuffer, vk_d.offsetIdx * sizeof(uint32_t), sizeof(idxFullscreenQuad) / sizeof(idxFullscreenQuad[0]) * sizeof(uint32_t), (void*)& idxFullscreenQuad[0]);
//	VK_UploadBufferDataOffset(&vk_d.vertexbuffer, vk_d.offset * sizeof(vec4_t), sizeof(vFullscreenQuad) / sizeof(vFullscreenQuad[0]) * sizeof(vec4_t), (void*)& vFullscreenQuad[0]);
//	VK_UploadBufferDataOffset(&vk_d.uvbuffer1, vk_d.offset * sizeof(vec2_t), sizeof(vFullscreenQuad) / sizeof(vFullscreenQuad[0]) * sizeof(vec2_t), (void*)& uvFullscreenQuad[0]);
//
//	VK_Bind1DescriptorSet(&vk_d.fullscreenRectPipeline, &image->descriptor_set);
//	VK_BindPipeline(&vk_d.fullscreenRectPipeline);
//	VK_DrawIndexed(&vk_d.indexbuffer, sizeof(idxFullscreenQuad) / sizeof(idxFullscreenQuad[0]), vk_d.offsetIdx, vk_d.offset);
//
//	vk_d.offsetIdx += sizeof(idxFullscreenQuad) / sizeof(idxFullscreenQuad[0]);
//	vk_d.offset += sizeof(vFullscreenQuad) / sizeof(vFullscreenQuad[0]);
//
//	// restore state
//	Com_Memcpy(&vk_d.viewport, &vpSave, sizeof(vk_d.viewport));
//	Com_Memcpy(&vk_d.scissor, &sSave, sizeof(vk_d.scissor));
//}

/*
** PIPELINE LIST
*/
//qboolean VK_FindPipeline() {
//	for (uint32_t i = 0; i < vk_d.pipelineListSize; ++i) {
//		render_state_s* state = &vk_d.pipelineList[i].state;
//		vkpipeline_t* found_p = &vk_d.pipelineList[i].pipeline;
//		if (memcmp(&vk_d.state, state, sizeof(render_state_s)) == 0) {
//			return i;
//		}
//	}
//	return -1;
//}
//uint32_t VK_AddPipeline(vkpipeline_t* p) {
//	if (vk_d.pipelineListSize >= VK_MAX_NUM_PIPELINES) {
//		ri.Error(ERR_FATAL, "Vulkan: Maximum amount of pipelines reached!");
//	}
//	vk_d.pipelineList[vk_d.pipelineListSize] = (vkpipe_t){ *p, vk_d.state };
//	vk_d.pipelineListSize++;
//	return vk_d.pipelineListSize - 1;
//}



// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#synchronization-access-types
//bool memory::accessFlagBitCompatibleWithStageFlags(VkAccessFlagBits accessFlagBit, VkPipelineStageFlags stageFlags)
//{
//	switch (accessFlagBit)
//	{
//	case VK_ACCESS_INDIRECT_COMMAND_READ_BIT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR)) return true;
//		break;
//	case VK_ACCESS_INDEX_READ_BIT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT)) return true;
//		break;
//	case VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT)) return true;
//		break;
//	case VK_ACCESS_UNIFORM_READ_BIT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV | VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV 
//			| VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT 
//			| VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT
//			| VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)) return true;
//		break;
//	case VK_ACCESS_SHADER_READ_BIT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV
//			| VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
//			| VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT
//			| VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
//			| VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)) return true;
//		break;
//	case VK_ACCESS_SHADER_WRITE_BIT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV | VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV
//			| VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
//			| VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT
//			| VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)) return true;
//		break;
//	case VK_ACCESS_INPUT_ATTACHMENT_READ_BIT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_2_SUBPASS_SHADING_BIT_HUAWEI | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)) return true;
//		break;
//	case VK_ACCESS_COLOR_ATTACHMENT_READ_BIT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)) return true;
//		break;
//	case VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)) return true;
//		break;
//	case VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT)) return true;
//		break;
//	case VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT)) return true;
//		break;
//	case VK_ACCESS_TRANSFER_READ_BIT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR)) return true;
//		break;
//	case VK_ACCESS_TRANSFER_WRITE_BIT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR)) return true;
//		break;
//	case VK_ACCESS_HOST_READ_BIT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_HOST_BIT)) return true;
//		break;
//	case VK_ACCESS_HOST_WRITE_BIT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_HOST_BIT)) return true;
//		break;
//	case VK_ACCESS_MEMORY_READ_BIT:
//	case VK_ACCESS_MEMORY_WRITE_BIT:
//		return true;
//	case VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)) return true;
//		break;
//	case VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV)) return true;
//		break;
//	case VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV)) return true;
//		break;
//	case VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT)) return true;
//		break;
//	case VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR)) return true;
//		break;
//	case VK_ACCESS_2_INVOCATION_MASK_READ_BIT_HUAWEI:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_2_INVOCATION_MASK_BIT_HUAWEI)) return true;
//		break;
//	case VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT)) return true;
//		break;
//	case VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT)) return true;
//		break;
//	case VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT)) return true;
//		break;
//	case VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV | VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV
//			| VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT
//			| VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT
//			| VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
//			| VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR)) return true;
//		break;
//	case VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR)) return true;
//		break;
//	case VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT:
//		if (IS_ONLY_ANY_BIT_SET(stageFlags, VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT)) return true;
//		break;
//	default:
//		return false;
//	}
//	return false;
//}

VkPipelineStageFlags rvk::memory::getSrcStageMask(VkImageLayout aOldImageLayout, VkImageLayout aNewImageLayout) {
	switch (aOldImageLayout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
		switch (aNewImageLayout)
		{
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
		case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
			return VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
		default:
			return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		}
	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		// define no dependent stage
		return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		return VK_PIPELINE_STAGE_TRANSFER_BIT;

	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		return VK_PIPELINE_STAGE_TRANSFER_BIT;

	case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
	case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
	case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
		return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
	case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
	case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
		return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	default:
		return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	}
}
VkPipelineStageFlags rvk::memory::getDstStageMask(VkImageLayout aOldImageLayout, VkImageLayout aNewImageLayout) {
	switch (aNewImageLayout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		// defines no dependent stage
		return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		return VK_PIPELINE_STAGE_TRANSFER_BIT;

	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		return VK_PIPELINE_STAGE_TRANSFER_BIT;

	case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
	case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
	case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
		return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
	case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
	case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
		return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	default:
		return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	}
}

// from Sascha Willems
// https://github.com/SaschaWillems/Vulkan/blob/master/base/VulkanTools.cpp
VkAccessFlags rvk::memory::getSrcAccessMask(VkImageLayout aOldImageLayout, VkImageLayout aNewImageLayout) {
	// Source layouts (old)
	// Source access mask controls actions that have to be finished on the old layout
	// before it will be transitioned to the new layout
	switch (aOldImageLayout)
	{
	// only valid on init
	case VK_IMAGE_LAYOUT_UNDEFINED:
		// Image layout is undefined (or does not matter)
		// Only valid as initial layout
		// No flags required, listed only for completeness
		// Image will be read in a shader (sampler, input attachment)
		// Make sure any writes to the image have been finished
		if (aNewImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) return VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		return 0;

	// only valid on init
	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		// Image is preinitialized
		// Only valid as initial layout for linear images, preserves memory contents
		// Make sure host writes have been finished
		return VK_ACCESS_HOST_WRITE_BIT;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		// Image is a color attachment
		// Make sure any writes to the color buffer have been finished
		return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		// Image is a depth/stencil attachment
		// Make sure any writes to the depth/stencil buffer have been finished
		return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		// Image is a transfer source
		// Make sure any reads from the image have been finished
		return VK_ACCESS_TRANSFER_READ_BIT;

	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		// Image is a transfer destination
		// Make sure any writes to the image have been finished
		return VK_ACCESS_TRANSFER_WRITE_BIT;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		// Image is read by a shader
		// Make sure any shader reads from the image have been finished
		return VK_ACCESS_SHADER_READ_BIT;

	default:
		// Other source layouts aren't handled (yet)
		return 0;
	}
}
VkAccessFlags rvk::memory::getDstAccessMask(VkImageLayout aOldImageLayout, VkImageLayout aNewImageLayout) {
	// Target layouts (new)
			// Destination access mask controls the dependency for the new image layout
	switch (aNewImageLayout)
	{
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		// Image will be used as a transfer destination
		// Make sure any writes to the image have been finished
		return VK_ACCESS_TRANSFER_WRITE_BIT;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		// Image will be used as a transfer source
		// Make sure any reads from the image have been finished
		return VK_ACCESS_TRANSFER_READ_BIT;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		// Image will be used as a color attachment
		// Make sure any writes to the color buffer have been finished
		return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		// Image layout will be used as a depth/stencil attachment
		// Make sure any writes to depth/stencil buffer have been finished
		return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		// Image will be read in a shader (sampler, input attachment)
		// Make sure any writes to the image have been finished
		return VK_ACCESS_SHADER_READ_BIT;

	default:
		// Other source layouts aren't handled (yet)
		return 0;
	}
}


/*
* Swapchain
*/
//void transitionCurrentImage(VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkCommandBuffer commandBuffer) {
//	VkImageSubresourceRange subresourceRange = {};
//	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//	subresourceRange.baseMipLevel = 0;
//	subresourceRange.levelCount = 1;
//	subresourceRange.layerCount = 1;
//
//	VkImageMemoryBarrier barrier = {};
//	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//	barrier.subresourceRange.baseArrayLayer = 0;
//	barrier.subresourceRange.layerCount = 1;
//	barrier.subresourceRange.baseMipLevel = 0;
//	barrier.subresourceRange.levelCount = 1;
//
//	barrier.oldLayout = oldImageLayout;
//	barrier.newLayout = newImageLayout;
//	barrier.srcAccessMask = rvk::memory::getSrcAccessMask(oldImageLayout, newImageLayout);
//	barrier.dstAccessMask = rvk::memory::getDstAccessMask(oldImageLayout, newImageLayout);
//	barrier.image = vk.swapchain.CurrentImage();
//
//	// if no cmd buffer is set, use single time cmd
//	bool stc = false;
//	if (commandBuffer == VK_NULL_HANDLE) stc = true;
//
//	if (stc) rvk::cmd::beginSingleTimeCommands(&commandBuffer);
//	vkCmdPipelineBarrier(commandBuffer,
//		srcStageMask,
//		dstStageMask,
//		0, 0, NULL, 0, NULL,
//		1, &barrier);
//	if (stc) rvk::cmd::endSingleTimeCommands(&commandBuffer);
//}

void rvk::swapchain::CMD_CopyImageToCurrentSwapchainImage(const VkCommandBuffer cb, rvk::Image *src_image) {

	//const VkImageLayout oldSrcLayout = src_image->getLayout();

	//VkImageMemoryBarrier barrier = {};
	//barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	//barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//barrier.subresourceRange.baseArrayLayer = 0;
	//barrier.subresourceRange.layerCount = 1;
	//barrier.subresourceRange.baseMipLevel = 0;
	//barrier.subresourceRange.levelCount = 1;
	//
	//// transition swapchain
	//barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	//barrier.srcAccessMask = rvk::memory::getSrcAccessMask(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	//barrier.dstAccessMask = rvk::memory::getDstAccessMask(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	//barrier.image = vk.swapchain.CurrentImage();
	//vkCmdPipelineBarrier(cb,
	//	VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	//	VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	//	0, 0, NULL, 0, NULL,
	//	1, &barrier);

	//// transition src image
	//barrier.oldLayout = oldSrcLayout;
	//barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	//barrier.srcAccessMask = rvk::memory::getSrcAccessMask(oldSrcLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	//barrier.dstAccessMask = rvk::memory::getDstAccessMask(oldSrcLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	//barrier.image = src_image->getHandle();
	//vkCmdPipelineBarrier(cb,
	//	VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	//	VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	//0, 0, NULL, 0, NULL,
	//1, &barrier);

	//// copy image
	//VkImageCopy copyRegion{};
	//copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	//copyRegion.srcOffset = { 0, 0, 0 };
	//copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	//copyRegion.dstOffset = { 0, 0, 0 };
	//copyRegion.extent = { vk.swapchain.extent.width, vk.swapchain.extent.height, 1 };
	//vkCmdCopyImage(cb, src_image->getHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vk.swapchain.CurrentImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

	//// transition swapchain back
	//barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	//barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	//barrier.srcAccessMask = rvk::memory::getSrcAccessMask(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	//barrier.dstAccessMask = rvk::memory::getDstAccessMask(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	//barrier.image = vk.swapchain.CurrentImage();
	//vkCmdPipelineBarrier(cb,
	//	VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	//	VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	//	0, 0, NULL, 0, NULL,
	//	1, &barrier);

	//// transition src image back
	//barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	//barrier.newLayout = oldSrcLayout;
	//barrier.srcAccessMask = rvk::memory::getSrcAccessMask(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, oldSrcLayout);
	//barrier.dstAccessMask = rvk::memory::getDstAccessMask(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, oldSrcLayout);
	//barrier.image = src_image->getHandle(); //vk.swapchain.CurrentImage();
	//vkCmdPipelineBarrier(cb,
	//	VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	//	VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	//	0, 0, NULL, 0, NULL,
	//	1, &barrier);
}

void rvk::swapchain::CMD_BlitImageToCurrentSwapchainImage(CommandBuffer* cb, Swapchain* sc, rvk::Image* src_image, const VkFilter filter) {
	LogicalDevice* mDevice = cb->getDevice();
	VkExtent2D sc_extent = sc->getExtent();
	const VkImageLayout oldSrcLayout = src_image->getLayout();

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;

	// transition swapchain
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcAccessMask = rvk::memory::getSrcAccessMask(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	barrier.dstAccessMask = rvk::memory::getDstAccessMask(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	barrier.image = sc->getCurrentImage()->getHandle();
	mDevice->vkCmd.PipelineBarrier(cb->getHandle(),
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		0, 0, NULL, 0, NULL,
		1, &barrier);

	// transition src image
	barrier.oldLayout = oldSrcLayout;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcAccessMask = rvk::memory::getSrcAccessMask(oldSrcLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	barrier.dstAccessMask = rvk::memory::getDstAccessMask(oldSrcLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	barrier.image = src_image->getHandle();
	if (oldSrcLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
		mDevice->vkCmd.PipelineBarrier(cb->getHandle(),
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			0, 0, NULL, 0, NULL,
			1, &barrier);
	}

	// copy image
	const VkExtent3D extent = src_image->getExtent();
	VkImageBlit blit = {};
	blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	blit.srcOffsets[0] = { 0, 0, 0 };
	blit.srcOffsets[1] = { (int)extent.width, (int)extent.height, 1 };
	blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	blit.dstOffsets[0] = { 0, 0, 0 };
	blit.dstOffsets[1] = { (int)sc_extent.width, (int)sc_extent.height, 1 };
	mDevice->vkCmd.BlitImage(cb->getHandle(), src_image->getHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sc->getCurrentImage()->getHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, filter);

	// transition swapchain back
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.srcAccessMask = rvk::memory::getSrcAccessMask(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	barrier.dstAccessMask = rvk::memory::getDstAccessMask(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	barrier.image = sc->getCurrentImage()->getHandle();
	mDevice->vkCmd.PipelineBarrier(cb->getHandle(),
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		0, 0, NULL, 0, NULL,
		1, &barrier);

	// transition src image back
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.newLayout = oldSrcLayout;
	barrier.srcAccessMask = rvk::memory::getSrcAccessMask(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, oldSrcLayout);
	barrier.dstAccessMask = rvk::memory::getDstAccessMask(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, oldSrcLayout);
	barrier.image = src_image->getHandle(); //vk.swapchain.CurrentImage();
	if (oldSrcLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
		mDevice->vkCmd.PipelineBarrier(cb->getHandle(),
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			0, 0, NULL, 0, NULL,
			1, &barrier);
	}
}

//void swapchain::readPixelScreen(const int32_t x, const int32_t y, const bool alpha, uint8_t* pixel)
//{
//	vkDeviceWaitIdle(vk.device);
//
//	// Create image to copy swapchain content on host
//	VkImageCreateInfo desc = {};
//	desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
//	desc.pNext = NULL;
//	desc.flags = 0;
//	desc.imageType = VK_IMAGE_TYPE_2D;
//	desc.format = VK_FORMAT_R8G8B8A8_UNORM;
//	desc.extent.width = 1;
//	desc.extent.height = 1;
//	desc.extent.depth = 1;
//	desc.mipLevels = 1;
//	desc.arrayLayers = 1;
//	desc.samples = VK_SAMPLE_COUNT_1_BIT;
//	desc.tiling = VK_IMAGE_TILING_LINEAR;
//	desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
//	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
//	desc.queueFamilyIndexCount = 0;
//	desc.pQueueFamilyIndices = NULL;
//	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//
//	VkImage image;
//	VK_CHECK(vkCreateImage(vk.device, &desc, NULL, &image), "failed to create image");
//
//	VkMemoryRequirements mRequirements;
//	vkGetImageMemoryRequirements(vk.device, image, &mRequirements);
//
//	VkMemoryAllocateInfo alloc_info = {};
//	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
//	alloc_info.pNext = NULL;
//	alloc_info.allocationSize = mRequirements.size;
//	alloc_info.memoryTypeIndex = findMemoryTypeIndex(mRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
//	ASSERT(alloc_info.memoryTypeIndex != -1, "could not find suitable Memory");
//
//	VkDeviceMemory memory;
//	VK_CHECK(vkAllocateMemory(vk.device, &alloc_info, NULL, &memory), "could not allocate Image Memory");
//	VK_CHECK(vkBindImageMemory(vk.device, image, memory, 0), "could not bind Image Memory");
//
//	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
//	rvk::cmd::beginSingleTimeCommands(&commandBuffer);
//
//	// transition dst image
//	VkImageMemoryBarrier barrier = {};
//	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//	barrier.subresourceRange.baseArrayLayer = 0;
//	barrier.subresourceRange.layerCount = 1;
//	barrier.subresourceRange.baseMipLevel = 0;
//	barrier.subresourceRange.levelCount = 1;
//
//	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
//	barrier.srcAccessMask = 0;
//	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//	barrier.image = image;
//
//	vkCmdPipelineBarrier(commandBuffer,
//		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
//		VK_PIPELINE_STAGE_TRANSFER_BIT,
//		0, 0, NULL, 0, NULL,
//		1, &barrier);
//
//	// transition swap chain image
//	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//	barrier.subresourceRange.baseArrayLayer = 0;
//	barrier.subresourceRange.layerCount = 1;
//	barrier.subresourceRange.baseMipLevel = 0;
//	barrier.subresourceRange.levelCount = 1;
//
//	barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
//	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
//	barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
//	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
//	barrier.image = vk.swapchain.images[vk.swapchain.currentImage];
//
//	vkCmdPipelineBarrier(commandBuffer,
//		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
//		VK_PIPELINE_STAGE_TRANSFER_BIT,
//		0, 0, NULL, 0, NULL,
//		1, &barrier);
//
//	VkImageCopy region = { 0 };
//	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//	region.srcSubresource.mipLevel = 0;
//	region.srcSubresource.baseArrayLayer = 0;
//	region.srcSubresource.layerCount = 1;
//	region.dstSubresource = region.srcSubresource;
//	region.srcOffset = { x, y, 0 };
//	region.dstOffset = { 0, 0, 0 };
//	region.extent.width = 1;
//	region.extent.height = 1;
//	region.extent.depth = 1;
//
//	vkCmdCopyImage(commandBuffer, vk.swapchain.images[vk.swapchain.currentImage], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
//		image, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
//
//	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
//	barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
//	barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
//	barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
//	barrier.image = vk.swapchain.images[vk.swapchain.currentImage];
//
//	vkCmdPipelineBarrier(commandBuffer,
//		VK_PIPELINE_STAGE_TRANSFER_BIT,
//		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
//		0, 0, NULL, 0, NULL,
//		1, &barrier);
//	rvk::cmd::endSingleTimeCommands(&commandBuffer);
//
//	// Copy data from destination image to memory buffer.
//	VkImageSubresource subresource = { 0 };
//	subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//	subresource.mipLevel = 0;
//	subresource.arrayLayer = 0;
//	VkSubresourceLayout layout = { 0 };
//	vkGetImageSubresourceLayout(vk.device, image, &subresource, &layout);
//
//	VkFormat format = vk.swapchain.imageFormat;
//	bool swizzleComponents = (format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SNORM);
//
//	uint8_t* data;
//	VK_CHECK(vkMapMemory(vk.device, memory, 0, VK_WHOLE_SIZE, 0, (void**)&data), "failed to map memory");
//	memcpy(pixel, data, alpha ? 4 : 3);
//
//	vkDestroyImage(vk.device, image, NULL);
//	vkFreeMemory(vk.device, memory, NULL);
//}

/*
* PERFORMANCE
*/
//typedef enum {
//    PROFILER_BUILD_AS_BEGIN,
//    PROFILER_BUILD_AS_END,
//    PROFILER_ASVGF_RNG_BEGIN,
//    PROFILER_ASVGF_RNG_END,
//    PROFILER_ASVGF_FORWARD_BEGIN,
//    PROFILER_ASVGF_FORWARD_END,
//    PROFILER_PRIMARY_RAYS_BEGIN,
//    PROFILER_PRIMARY_RAYS_END,
//    PROFILER_REFLECTION_REFRACTION_BEGIN,
//    PROFILER_REFLECTION_REFRACTION_END,
//    PROFILER_DIRECT_ILLUMINATION_BEGIN,
//    PROFILER_DIRECT_ILLUMINATION_END,
//    PROFILER_INDIRECT_ILLUMINATION_BEGIN,
//    PROFILER_INDIRECT_ILLUMINATION_END,
//    PROFILER_ASVGF_GRADIENT_BEGIN,
//    PROFILER_ASVGF_GRADIENT_END,
//    PROFILER_ASVGF_GRADIENT_ATROUS_BEGIN,
//    PROFILER_ASVGF_GRADIENT_ATROUS_END,
//    PROFILER_ASVGF_TEMPORAL_BEGIN,
//    PROFILER_ASVGF_TEMPORAL_END,
//    PROFILER_ASVGF_ATROUS_BEGIN,
//    PROFILER_ASVGF_ATROUS_END,
//    PROFILER_ASVGF_TAA_BEGIN,
//    PROFILER_ASVGF_TAA_END,
//    PROFILER_IN_FLIGHT
//} vkprofiler_e;
//void VK_SetPerformanceMarker(VkCommandBuffer command_buffer, int index) {
//    vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
//        vk.queryPool, (vk.swapchain.currentImage * PROFILER_IN_FLIGHT) + index);
//}

//void VK_ResetPerformanceQueryPool(VkCommandBuffer command_buffer) {
//    vkCmdResetQueryPool(command_buffer, vk.queryPool,
//        PROFILER_IN_FLIGHT * vk.swapchain.currentImage,
//        PROFILER_IN_FLIGHT);
//}
//void setupQueryPool() {
//    VkQueryPoolCreateInfo createInfo = {
//        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
//        NULL,
//        0,
//        VK_QUERY_TYPE_TIMESTAMP,
//        VK_MAX_SWAPCHAIN_SIZE * PROFILER_IN_FLIGHT,
//        0
//    };
//    VK_CHECK(vkCreateQueryPool(vk.device, &createInfo, NULL, &vk.queryPool), "failed to create queryPool!");
//}

//void VK_PerformanceQueryPoolResults() {
//	VK_CHECK(vkGetQueryPoolResults(vk.device, vk.queryPool,
//		PROFILER_IN_FLIGHT * vk.swapchain.currentImage,
//		PROFILER_IN_FLIGHT,
//		sizeof(vk_d.queryPoolResults[0]),
//		&(vk_d.queryPoolResults[vk.swapchain.currentImage][0]),
//		sizeof(vk_d.queryPoolResults[0][0]),
//		VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT), "failed to get Query results!");
//}
//void VK_TimeBetweenMarkers(double* ms, int start, int end) {
//	*ms = (double)(vk_d.queryPoolResults[vk.swapchain.currentImage][end] - vk_d.queryPoolResults[vk.swapchain.currentImage][start]) * 1e-6;
//}

const char* rvk::error::errorString(const VkResult aErrorCode)
{
	switch (aErrorCode)
	{
#define STR(r) case VK_ ##r: return #r
        STR(SUCCESS);
        STR(NOT_READY);
        STR(TIMEOUT);
        STR(EVENT_SET);
        STR(EVENT_RESET);
        STR(INCOMPLETE);
        STR(ERROR_OUT_OF_HOST_MEMORY);
        STR(ERROR_OUT_OF_DEVICE_MEMORY);
        STR(ERROR_INITIALIZATION_FAILED);
        STR(ERROR_DEVICE_LOST);
        STR(ERROR_MEMORY_MAP_FAILED);
        STR(ERROR_LAYER_NOT_PRESENT);
        STR(ERROR_EXTENSION_NOT_PRESENT);
        STR(ERROR_FEATURE_NOT_PRESENT);
        STR(ERROR_INCOMPATIBLE_DRIVER);
        STR(ERROR_TOO_MANY_OBJECTS);
        STR(ERROR_FORMAT_NOT_SUPPORTED);
        STR(ERROR_FRAGMENTED_POOL);
        STR(ERROR_OUT_OF_POOL_MEMORY);
        STR(ERROR_INVALID_EXTERNAL_HANDLE);
        STR(ERROR_SURFACE_LOST_KHR);
        STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
        STR(SUBOPTIMAL_KHR);
        STR(ERROR_OUT_OF_DATE_KHR);
        STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
        STR(ERROR_VALIDATION_FAILED_EXT);
        STR(ERROR_INVALID_SHADER_NV);
        STR(ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
        STR(ERROR_FRAGMENTATION_EXT);
        STR(ERROR_NOT_PERMITTED_EXT);
        STR(ERROR_INVALID_DEVICE_ADDRESS_EXT);
        STR(ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
#undef STR
	default:
		return "UNKNOWN_ERROR";
	}
}

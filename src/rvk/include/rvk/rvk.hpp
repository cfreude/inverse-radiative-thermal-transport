#pragma once
#include <rvk/parts/rvk_public.hpp>
#include <rvk/parts/logger.hpp>

#include <rvk/parts/instance.hpp>
#include <rvk/parts/device_physical.hpp>
#include <rvk/parts/device_logical.hpp>
#include <rvk/parts/swapchain.hpp>
#include <rvk/parts/command_pool.hpp>
#include <rvk/parts/command_buffer.hpp>
#include <rvk/parts/queue.hpp>
#include <rvk/parts/queue_family.hpp>
#include <rvk/parts/fence.hpp>

#include <rvk/parts/buffer.hpp>
#include <rvk/parts/sampler.hpp>
#include <rvk/parts/image.hpp>
#include <rvk/parts/acceleration_structure.hpp>
#include <rvk/parts/acceleration_structure_bottom_level.hpp>
#include <rvk/parts/acceleration_structure_top_level.hpp>
#include <rvk/parts/descriptor.hpp>

#include <rvk/parts/pipeline.hpp>
#include <rvk/parts/pipeline_compute.hpp>
#include <rvk/parts/pipeline_rasterize.hpp>
#include <rvk/parts/pipeline_ray_trace.hpp>

#include <rvk/parts/framebuffer.hpp>
#include <rvk/parts/renderpass.hpp>

#include <rvk/parts/shader.hpp>
#include <rvk/parts/shader_compute.hpp>
#include <rvk/parts/shader_rasterize.hpp>
#include <rvk/parts/shader_ray_trace.hpp>

#include <rvk/parts/cuda.hpp>

RVK_BEGIN_NAMESPACE

namespace memory {
	//bool											accessFlagBitCompatibleWithStageFlags(VkAccessFlagBits accessFlagBit, VkPipelineStageFlags stageFlags);
	VkPipelineStageFlags							getSrcStageMask(VkImageLayout aOldImageLayout, VkImageLayout aNewImageLayout);
	VkPipelineStageFlags							getDstStageMask(VkImageLayout aOldImageLayout, VkImageLayout aNewImageLayout);
	VkAccessFlags									getSrcAccessMask(VkImageLayout aOldImageLayout, VkImageLayout aNewImageLayout);
	VkAccessFlags									getDstAccessMask(VkImageLayout aOldImageLayout, VkImageLayout aNewImageLayout);
}
namespace swapchain {
	//void                                            readPixelScreen(const int32_t x, const int32_t y, const bool alpha, uint8_t *pixel);
	uint8_t*										readPixelsScreen(SingleTimeCommand* stc, Swapchain* sc, bool alpha);
	void											CMD_CopyImageToCurrentSwapchainImage(VkCommandBuffer cb, rvk::Image* src_image);
	void											CMD_BlitImageToCurrentSwapchainImage(CommandBuffer* cb, Swapchain*sc, rvk::Image* src_image, const VkFilter filter);
													// get resources related to current swapchain image
}

namespace error {
    const char*										errorString(VkResult aErrorCode);
}

constexpr void VK_CHECK(const VkResult aResult, const std::string& aString)
{
	if (aResult < 0) {
		Logger::error("Vulkan: " + aString + " " + rvk::error::errorString(aResult));
	}
}


RVK_END_NAMESPACE

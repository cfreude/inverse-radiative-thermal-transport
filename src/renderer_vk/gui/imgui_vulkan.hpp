#pragma once
#include <rvk/rvk.hpp>
namespace ImGui_Vulkan_Impl {
													// for setup
	VkDescriptorPool								setup(rvk::CommandPool* aCommandPool, rvk::Queue* aQueue, rvk::Swapchain* aSc);
	void											destroy(const rvk::LogicalDevice* aDevice, VkDescriptorPool& aPool);
													
													// for each frame
	void											prepare(VkExtent2D aExtent);
	void											draw(const rvk::CommandBuffer* aCommandBuffer);
}

#pragma once
#include <rvk/parts/rvk_public.hpp>

RVK_BEGIN_NAMESPACE
class LogicalDevice;
class CommandPool;
class Queue;
class Buffer;
class CommandBuffer
{
public:
												CommandBuffer(LogicalDevice* aDevice, CommandPool* aCommandPool, VkCommandBufferLevel aLevel);
												CommandBuffer(LogicalDevice* aDevice, CommandPool* aCommandPool, VkCommandBufferLevel aLevel, VkCommandBuffer aCommandBuffer);
												~CommandBuffer() = default;

	void										begin(VkCommandBufferUsageFlags aFlags);
	void										end();
	void										reset(VkCommandBufferResetFlags aFlags = 0) const;
	VkCommandBuffer								getHandle() const;
	LogicalDevice*								getDevice() const;
	uint32_t									getQueueFamilyIndex() const;
	CommandPool*								getPool() const;


	void										cmdMemoryBarrier(VkPipelineStageFlags aSrcStageMask, VkPipelineStageFlags aDstStageMask, VkAccessFlags aSrcAccessMask = 0, VkAccessFlags aDstAccessMask = 0);
	void										cmdBufferMemoryBarrier(const Buffer* aBuffer, VkPipelineStageFlags aSrcStageMask, VkPipelineStageFlags aDstStageMask, VkAccessFlags aSrcAccessMask = 0, VkAccessFlags aDstAccessMask = 0);

private:
	friend class CommandPool;
	bool										mRecording;
	VkCommandBufferUsageFlags					mFlags;
	LogicalDevice*								mDevice;
	VkCommandBufferLevel						mLevel;
	CommandPool*								mCommandPool;
	VkCommandBuffer								mCommandBuffer;
};

class SingleTimeCommand
{
public:
							SingleTimeCommand(CommandPool* aCommandPool, Queue* aQueue);
							~SingleTimeCommand();
	void					begin();
	CommandBuffer*			buffer() const;
	void					end();
							// or execute directly
	void					execute(const std::function<void(CommandBuffer*)>& aFunction);

private:
	CommandPool*			mCommandPool;
	Queue*					mQueue;

	CommandBuffer*			mCommandBuffer;
};
RVK_END_NAMESPACE
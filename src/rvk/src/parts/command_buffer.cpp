#include <rvk/parts/command_buffer.hpp>
#include "rvk_private.hpp"

RVK_USE_NAMESPACE

CommandBuffer::CommandBuffer(LogicalDevice* aDevice, CommandPool* aCommandPool, const VkCommandBufferLevel aLevel) :
mRecording(false), mFlags(0), mDevice(aDevice), mLevel(aLevel), mCommandPool(aCommandPool), mCommandBuffer(VK_NULL_HANDLE)
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = aLevel;
	allocInfo.commandPool = mCommandPool->getHandle();
	allocInfo.commandBufferCount = 1;
	VK_CHECK(mDevice->vk.AllocateCommandBuffers(mDevice->getHandle(), &allocInfo, &mCommandBuffer), "failed to allocate CMD Buffer!");
}

CommandBuffer::CommandBuffer(LogicalDevice* aDevice, CommandPool* aCommandPool, const VkCommandBufferLevel aLevel, const VkCommandBuffer aCommandBuffer) :
mRecording(false), mFlags(0), mDevice(aDevice), mLevel(aLevel), mCommandPool(aCommandPool), mCommandBuffer(aCommandBuffer)
{}

void CommandBuffer::begin(const VkCommandBufferUsageFlags aFlags)
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = aFlags;
	VK_CHECK(mDevice->vk.BeginCommandBuffer(mCommandBuffer, &beginInfo), "failed to begin CMD Buffer!");
		mFlags = aFlags;
	mRecording = true;
}

void CommandBuffer::end()
{
	VK_CHECK(mDevice->vk.EndCommandBuffer(mCommandBuffer), "failed to end CMD Buffer!");
		mFlags = 0;
	mRecording = false;
}

void CommandBuffer::reset(const VkCommandBufferResetFlags aFlags) const
{
	mDevice->vk.ResetCommandBuffer(mCommandBuffer, aFlags);
}

VkCommandBuffer CommandBuffer::getHandle() const
{
	return mCommandBuffer;
}

LogicalDevice* CommandBuffer::getDevice() const
{
	return mDevice;
}

uint32_t CommandBuffer::getQueueFamilyIndex() const
{
	return mCommandPool->getQueueFamilyIndex();
}

CommandPool* CommandBuffer::getPool() const
{
	return mCommandPool;
}

void CommandBuffer::cmdMemoryBarrier(VkPipelineStageFlags aSrcStageMask, VkPipelineStageFlags aDstStageMask,
	VkAccessFlags aSrcAccessMask, VkAccessFlags aDstAccessMask)
{
	VkMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = aSrcAccessMask;
	barrier.dstAccessMask = aDstAccessMask;
	mDevice->vkCmd.PipelineBarrier(mCommandBuffer,
		aSrcStageMask,
		aDstStageMask,
		0,
		1, &barrier,
		0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
}

void CommandBuffer::cmdBufferMemoryBarrier(const Buffer* aBuffer, VkPipelineStageFlags aSrcStageMask,
	VkPipelineStageFlags aDstStageMask, VkAccessFlags aSrcAccessMask, VkAccessFlags aDstAccessMask)
{
	VkBufferMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.buffer = aBuffer->getRawHandle();
	barrier.srcAccessMask = aSrcAccessMask;
	barrier.dstAccessMask = aDstAccessMask;
	barrier.size = aBuffer->getSize();
	mDevice->vkCmd.PipelineBarrier(mCommandBuffer,
		aSrcStageMask,
		aDstStageMask,
		0,
		0, VK_NULL_HANDLE,
		1, &barrier,
		0, VK_NULL_HANDLE);
}

SingleTimeCommand::SingleTimeCommand(CommandPool* aCommandPool, Queue* aQueue) :
mCommandPool(aCommandPool), mQueue(aQueue), mCommandBuffer(nullptr)
{
}

SingleTimeCommand::~SingleTimeCommand()
{
	if (mCommandBuffer) end();
}

void SingleTimeCommand::begin()
{
	if (mCommandBuffer) end();
	mCommandBuffer = mCommandPool->allocCommandBuffers(1).front();
	mCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
}

CommandBuffer* SingleTimeCommand::buffer() const
{
	return mCommandBuffer;
}

void SingleTimeCommand::end()
{
	mCommandBuffer->end();
	mQueue->submitCommandBuffers({ mCommandBuffer });
	mQueue->waitIdle();
	mCommandPool->freeCommandBuffers({ mCommandBuffer });
	mCommandBuffer = nullptr;
}

void SingleTimeCommand::execute(const std::function<void(CommandBuffer*)>& aFunction)
{
	begin();
	aFunction(mCommandBuffer);
	end();
}

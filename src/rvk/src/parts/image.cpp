#include <rvk/parts/image.hpp>
#include "rvk_private.hpp"

RVK_USE_NAMESPACE

Image::Image(LogicalDevice* aDevice) : mDevice(aDevice), mExtent({ 0, 0, 0 }), mFormat(VK_FORMAT_UNDEFINED), mProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), mAspect(VK_IMAGE_ASPECT_COLOR_BIT), mMipLevels(0),
mArrayLayers(0), mLayout(VK_IMAGE_LAYOUT_UNDEFINED), mUsageFlags(0), mImage(VK_NULL_HANDLE), mImageView(VK_NULL_HANDLE), mSampler(VK_NULL_HANDLE), mMemory(VK_NULL_HANDLE) {}


// TODO: rewrite image layouts and transitions
rvk::Image::~Image() {
	destroy();
}

bool Image::isInit() const
{ return mImage != VK_NULL_HANDLE; }

VkImage Image::getHandle() const
{ return mImage; }

VkSampler Image::getSampler() const
{ return mSampler; }

VkImageLayout Image::getLayout() const
{ return mLayout; }

VkExtent3D Image::getExtent() const
{ return mExtent; }

uint32_t Image::getArrayLayerCount() const
{ return mArrayLayers; }

uint32_t Image::getMipMapCount() const
{ return mMipLevels; }

void rvk::Image::createImage2D(const uint32_t aWidth, const uint32_t aHeight, const VkFormat aFormat, 
                               const VkImageUsageFlags aUsage, const uint32_t aMipLevels, const uint32_t aArrayLayers) {
	mExtent = { aWidth, aHeight, 1 };
	mFormat = aFormat;
	mMipLevels = aMipLevels;
	mArrayLayers = aArrayLayers;
	mUsageFlags = aUsage;
	mProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	createImage(VK_IMAGE_TYPE_2D, aUsage);
	if(aArrayLayers == 1)createImageView(VK_IMAGE_VIEW_TYPE_2D);
	else if (aArrayLayers > 1)createImageView(VK_IMAGE_VIEW_TYPE_2D_ARRAY);
}

void rvk::Image::setSampler(const SamplerConfig aSamplerConfig)
{
	mSampler = mDevice->getSampler(aSamplerConfig)->getHandle();
}

void Image::setSampler(const VkSampler aSampler)
{ mSampler = aSampler; }

void Image::CMD_ClearColor(const CommandBuffer* aCmdBuffer, const float aR, const float aG, const float aB, const float aA) const
{
	if (!isBitSet<VkBufferUsageFlags>(mUsageFlags, VK_IMAGE_USAGE_TRANSFER_DST_BIT)) Logger::warning("Image needs VK_IMAGE_USAGE_TRANSFER_DST_BIT to be set to allow clearColor cmd");
	const VkClearColorValue cv = { aR, aG, aB, aA };
	VkImageSubresourceRange sr = {};
	sr.layerCount = mArrayLayers;
	sr.levelCount = mMipLevels;
	sr.aspectMask = mAspect;
	aCmdBuffer->getDevice()->vkCmd.ClearColorImage(aCmdBuffer->getHandle(), mImage, mLayout, &cv, 1, &sr);
}

void Image::CMD_TransitionImage(const CommandBuffer* aCmdBuffer, VkImageLayout aNewLayout)
{
	VkImageMemoryBarrier barrier = { };
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.subresourceRange.aspectMask = mAspect;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.levelCount = mMipLevels;
	barrier.subresourceRange.layerCount = mArrayLayers;

	if (aNewLayout == VK_IMAGE_LAYOUT_UNDEFINED) aNewLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcAccessMask = memory::getSrcAccessMask(mLayout, aNewLayout);
	barrier.dstAccessMask = memory::getDstAccessMask(mLayout, aNewLayout);

	barrier.oldLayout = mLayout;
	barrier.newLayout = aNewLayout;
	barrier.image = mImage;

	aCmdBuffer->getDevice()->vkCmd.PipelineBarrier(aCmdBuffer->getHandle(),
		memory::getSrcStageMask(mLayout, aNewLayout),
		memory::getDstStageMask(mLayout, aNewLayout),
		0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);
	mLayout = aNewLayout;
}

void Image::CMD_CopyBufferToImage(const CommandBuffer* aCmdBuffer, const Buffer* aSrcBuffer, const uint32_t aMipLevel,
                                  const uint32_t aArrayLayer, VkExtent3D aImageExtent, const VkOffset3D aImageOffset, const VkDeviceSize aBufferOffset) const
{
	if (aImageExtent.width == 0 && aImageExtent.height == 0 && aImageExtent.depth == 0) aImageExtent = mExtent;

	VkBufferImageCopy region = {};
	region.bufferOffset = aBufferOffset;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = mAspect;
	region.imageSubresource.mipLevel = aMipLevel;
	region.imageSubresource.baseArrayLayer = aArrayLayer;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = aImageOffset;
	region.imageExtent = aImageExtent;
	mDevice->vkCmd.CopyBufferToImage(aCmdBuffer->getHandle(), aSrcBuffer->getRawHandle(), mImage, mLayout, 1, &region);
}

void Image::CMD_CopyImageToBuffer(const CommandBuffer* aCmdBuffer, const Buffer* aDstBuffer, const uint32_t aMipLevel,
	uint32_t aArrayLayer, VkExtent3D aImageExtent, const VkOffset3D aImageOffset, const VkDeviceSize aBufferOffset) const
{
	if (aImageExtent.width == 0 && aImageExtent.height == 0 && aImageExtent.depth == 0) aImageExtent = mExtent;

	VkBufferImageCopy region = {};
	region.bufferOffset = aBufferOffset;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = mAspect;
	region.imageSubresource.mipLevel = aMipLevel;
	region.imageSubresource.baseArrayLayer = aArrayLayer;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = aImageOffset;
	region.imageExtent = aImageExtent;
	mDevice->vkCmd.CopyImageToBuffer(aCmdBuffer->getHandle(), mImage, mLayout, aDstBuffer->getRawHandle(), 1, &region);
}

//void Image::CMD_CopyImageToImage(const CommandBuffer* aCmdBuffer, const Image* aSrcImage)
//{
//	VkImageCopy region = {};
//	region.srcSubresource.aspectMask = aSrcImage->mAspect;
//	region.srcSubresource.mipLevel = 0;
//	region.srcSubresource.baseArrayLayer = 0;
//	region.srcSubresource.layerCount = 1;
//	region.dstSubresource = region.srcSubresource;
//	region.dstOffset = region.srcOffset;
//	//region.extent.width = extent.width;
//	//region.extent.height = extent.height;
//	region.extent.depth = 1;
//
//	mDevice->vkCmd.CopyImage(aCmdBuffer->getHandle(), aSrcImage->mImage, aSrcImage->mLayout,
//		mImage, mLayout, 1, &region);
//}

void Image::CMD_GenerateMipmaps(const CommandBuffer* aCmdBuffer) const
{
	VkImageLayout old_layout = mLayout;
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = mImage;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = mAspect;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mMipLevels;
	barrier.oldLayout = old_layout;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcAccessMask = memory::getSrcAccessMask(barrier.oldLayout, barrier.newLayout);
	barrier.dstAccessMask = memory::getDstAccessMask(barrier.oldLayout, barrier.newLayout);
	mDevice->vkCmd.PipelineBarrier(aCmdBuffer->getHandle(),
		memory::getSrcStageMask(barrier.oldLayout, barrier.newLayout),
		memory::getDstStageMask(barrier.oldLayout, barrier.newLayout),
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier);

	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = mExtent.width;
	int32_t mipHeight = mExtent.height;

	for (uint32_t i = 1; i < mMipLevels; i++) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = memory::getSrcAccessMask(barrier.oldLayout, barrier.newLayout);
		barrier.dstAccessMask = memory::getDstAccessMask(barrier.oldLayout, barrier.newLayout);

		mDevice->vkCmd.PipelineBarrier(aCmdBuffer->getHandle(),
			memory::getSrcStageMask(barrier.oldLayout, barrier.newLayout),
			memory::getDstStageMask(barrier.oldLayout, barrier.newLayout),
			0, 0, nullptr, 0, nullptr, 1, &barrier);

		VkImageBlit blit{};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = mAspect;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		mDevice->vkCmd.BlitImage(aCmdBuffer->getHandle(),
			mImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit, VK_FILTER_LINEAR);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = old_layout;
		barrier.srcAccessMask = memory::getSrcAccessMask(barrier.oldLayout, barrier.newLayout);
		barrier.dstAccessMask = memory::getDstAccessMask(barrier.oldLayout, barrier.newLayout);

		mDevice->vkCmd.PipelineBarrier(aCmdBuffer->getHandle(),
			memory::getSrcStageMask(barrier.oldLayout, barrier.newLayout),
			memory::getDstStageMask(barrier.oldLayout, barrier.newLayout),
			0, 0, nullptr, 0, nullptr, 1, &barrier);

		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}

	barrier.subresourceRange.baseMipLevel = mMipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = old_layout;
	barrier.srcAccessMask = memory::getSrcAccessMask(barrier.oldLayout, barrier.newLayout);
	barrier.dstAccessMask = memory::getDstAccessMask(barrier.oldLayout, barrier.newLayout);

	mDevice->vkCmd.PipelineBarrier(aCmdBuffer->getHandle(),
		memory::getSrcStageMask(barrier.oldLayout, barrier.newLayout),
		memory::getDstStageMask(barrier.oldLayout, barrier.newLayout),
		0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void Image::CMD_BlitImage(const CommandBuffer* aCmdBuffer, const rvk::Image* aSrcImage, const VkFilter aFilter) const
{
	const VkExtent3D srcExtent = aSrcImage->getExtent();
	const VkExtent3D dstExtent = mExtent;
	VkImageBlit blit = {};
	blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	blit.srcOffsets[0] = { 0, 0, 0 };
	blit.srcOffsets[1] = { static_cast<int32_t>(srcExtent.width), static_cast<int32_t>(srcExtent.height), 1 };
	blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	blit.dstOffsets[0] = { 0, 0, 0 };
	blit.dstOffsets[1] = { static_cast<int32_t>(dstExtent.width), static_cast<int32_t>(dstExtent.height), 1 };
	mDevice->vkCmd.BlitImage(aCmdBuffer->getHandle(), aSrcImage->getHandle(), aSrcImage->getLayout(), mImage, mLayout, 1, &blit, aFilter);
}

void Image::CMD_ImageBarrier(const CommandBuffer* aCmdBuffer, const VkPipelineStageFlags aSrcStage, const VkPipelineStageFlags aDstStage,
                             const VkAccessFlags aSrcAccess, const VkAccessFlags aDstAccess, const VkDependencyFlags aDependency) const
{
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = mImage;
	barrier.srcAccessMask = aSrcAccess;
	barrier.dstAccessMask = aDstAccess;
	barrier.oldLayout = mLayout;
	barrier.newLayout = mLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = mAspect;
	barrier.subresourceRange.levelCount = mMipLevels;
	barrier.subresourceRange.layerCount = mArrayLayers;
	mDevice->vkCmd.PipelineBarrier(aCmdBuffer->getHandle(),
		aSrcStage,
		aDstStage,
		aDependency,
		0, VK_NULL_HANDLE,
		0, VK_NULL_HANDLE,
		1, &barrier);
}

void Image::STC_UploadData2D(SingleTimeCommand* aStc, const uint32_t aWidth, const uint32_t aHeight, const uint32_t aBytesPerPixel,
                             const void* aDataIn, uint32_t aMipLevel, uint32_t aArrayLayer)
{
	const VkDeviceSize imageSize = static_cast<uint64_t>(aWidth) * static_cast<uint64_t>(aHeight) * static_cast<uint64_t>(1) * static_cast<uint64_t>(aBytesPerPixel);

	Buffer stagingBuffer(mDevice);
	stagingBuffer.create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, imageSize, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// write data to buffer
	stagingBuffer.mapBuffer();
	uint8_t* p = stagingBuffer.getMemoryPointer();
	std::memcpy(p, aDataIn, (size_t)(imageSize));
	stagingBuffer.unmapBuffer();
	STC_CopyBufferToImage(aStc, &stagingBuffer);
}

void Image::STC_DownloadData2D(SingleTimeCommand* aStc, uint32_t aWidth, uint32_t aHeight, uint32_t aBytesPerPixel,
	void* aDataOut, uint32_t aArrayLayer)
{
	const VkDeviceSize imageSize = static_cast<uint64_t>(aWidth) * static_cast<uint64_t>(aHeight) * static_cast<uint64_t>(1) * static_cast<uint64_t>(aBytesPerPixel);

	Buffer stagingBuffer(mDevice);
	stagingBuffer.create(VK_BUFFER_USAGE_TRANSFER_DST_BIT, imageSize, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	STC_CopyImageToBuffer(aStc, &stagingBuffer);

	// write data to buffer
	stagingBuffer.mapBuffer();
	uint8_t* p = stagingBuffer.getMemoryPointer();
	std::memcpy(aDataOut, p, (size_t)(imageSize));
	stagingBuffer.unmapBuffer();
}

void Image::STC_GenerateMipmaps(SingleTimeCommand* aStc) const
{
	aStc->begin();
	CMD_GenerateMipmaps(aStc->buffer());
	aStc->end();
}

void Image::STC_TransitionImage(SingleTimeCommand* aStc, VkImageLayout aNewLayout)
{
	aStc->begin();
	CMD_TransitionImage(aStc->buffer(), aNewLayout);
	aStc->end();
}

void rvk::Image::destroy()
{
	if (mImage != VK_NULL_HANDLE) {
		mDevice->vk.DestroyImage(mDevice->getHandle(), mImage, VK_NULL_HANDLE);
		mImage = VK_NULL_HANDLE;
	}
	if (mImageView != VK_NULL_HANDLE) {
		mDevice->vk.DestroyImageView(mDevice->getHandle(), mImageView, VK_NULL_HANDLE);
		mImageView = VK_NULL_HANDLE;
	}
	if (mMemory != VK_NULL_HANDLE) {
		mDevice->vk.FreeMemory(mDevice->getHandle(), mMemory, VK_NULL_HANDLE);
		mMemory = VK_NULL_HANDLE;
	}
	mSampler = VK_NULL_HANDLE;
	mExtent = { 0, 0, 0 };
	mFormat = VK_FORMAT_UNDEFINED;
	mAspect = 0;
	mMipLevels = 0;
	mArrayLayers = 0;
	mLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	mUsageFlags = 0;
}

/**
* Private 
**/
void rvk::Image::createImage(const VkImageType aImageType, const VkImageUsageFlags aUsage)
{
	VkImageCreateInfo desc = {};
	desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	desc.pNext = VK_NULL_HANDLE;
	desc.flags = 0;
	desc.imageType = aImageType;
	desc.format = mFormat;
	desc.extent.width = mExtent.width;
	desc.extent.height = mExtent.height;
	desc.extent.depth = 1;
	desc.mipLevels = mMipLevels;
	desc.arrayLayers = mArrayLayers;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	desc.usage = aUsage;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = VK_NULL_HANDLE;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK(mDevice->vk.CreateImage(mDevice->getHandle(), &desc, VK_NULL_HANDLE, &mImage), "failed to create Image!");
	createImageMemory();
}
void rvk::Image::createImageView(const VkImageViewType aViewType)
{
	switch (mFormat) {
	case VK_FORMAT_D16_UNORM_S8_UINT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		mAspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT; break;
	case VK_FORMAT_D16_UNORM:
	case VK_FORMAT_D32_SFLOAT:
		mAspect = VK_IMAGE_ASPECT_DEPTH_BIT; break;
	case VK_FORMAT_S8_UINT:
		mAspect = VK_IMAGE_ASPECT_DEPTH_BIT; break;
	default:
		mAspect = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	VkImageViewCreateInfo desc = {};
	desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	desc.pNext = VK_NULL_HANDLE;
	desc.flags = 0;
	desc.image = mImage;
	desc.viewType = aViewType;
	desc.format = mFormat;
	desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	desc.subresourceRange.aspectMask = mAspect;
	desc.subresourceRange.baseMipLevel = 0;
	desc.subresourceRange.levelCount = mMipLevels;//VK_REMAINING_MIP_LEVELS;
	desc.subresourceRange.baseArrayLayer = 0;
	desc.subresourceRange.layerCount = mArrayLayers;
	VK_CHECK(mDevice->vk.CreateImageView(mDevice->getHandle(), &desc, VK_NULL_HANDLE, &mImageView), "failed to create Image View!");
}

void Image::STC_CopyBufferToImage(SingleTimeCommand* aStc, const Buffer* aBuffer, uint32_t aMipLevel,
	uint32_t aArrayLayer, VkExtent3D aImageExtent, VkOffset3D aImageOffset, VkDeviceSize aBufferOffset)
{
	const VkImageLayout oldLayout = mLayout;
	aStc->begin();
	CMD_TransitionImage(aStc->buffer(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	CMD_CopyBufferToImage(aStc->buffer(), aBuffer, aMipLevel, aArrayLayer, aImageExtent, aImageOffset, aBufferOffset);
	CMD_TransitionImage(aStc->buffer(), oldLayout);
	aStc->end();
}

void Image::STC_CopyImageToBuffer(SingleTimeCommand* aStc, const Buffer* aBuffer, uint32_t aMipLevel,
	uint32_t ArrayLayer, VkExtent3D aImageExtent, VkOffset3D aImageOffset, VkDeviceSize aBufferOffset)
{
	const VkImageLayout oldLayout = mLayout;
	aStc->begin();
	CMD_TransitionImage(aStc->buffer(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	CMD_CopyImageToBuffer(aStc->buffer(), aBuffer, aMipLevel, ArrayLayer, aImageExtent, aImageOffset, aBufferOffset);
	CMD_TransitionImage(aStc->buffer(), oldLayout);
	aStc->end();
}

void Image::createImageMemory() {
	VkMemoryRequirements memRequirements = {};
	mDevice->vk.GetImageMemoryRequirements(mDevice->getHandle(), mImage, &memRequirements);

	const VkMemoryAllocateInfo memAllocInfo = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		VK_NULL_HANDLE,
		memRequirements.size,
		static_cast<uint32_t>(mDevice->findMemoryTypeIndex(mProperties, memRequirements.memoryTypeBits))
	};
	ASSERT(memAllocInfo.memoryTypeIndex != -1, "could not find suitable Memory");

	VK_CHECK(mDevice->vk.AllocateMemory(mDevice->getHandle(), &memAllocInfo, NULL, &mMemory), "failed to allocate Image Memory!");
	VK_CHECK(mDevice->vk.BindImageMemory(mDevice->getHandle(), mImage, mMemory, 0), "failed to bind Image Memory!");
}

//
//void VK_CreateCubeMap(vkimage_t* image, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, uint32_t mipLevels, uint32_t arrayLayers) {
//	image->extent = VkExtent3D{ width, height, 1 };
//	image->mipLevels = mipLevels;
//	image->arrayLayers = arrayLayers;
//
//	// create image
//	{
//		VkImageCreateInfo desc = {};
//		desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
//		desc.pNext = NULL;
//		desc.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
//		desc.imageType = VK_IMAGE_TYPE_2D;
//		desc.format = format;
//		desc.extent.width = width;
//		desc.extent.height = height;
//		desc.extent.depth = 1;
//		desc.mipLevels = image->mipLevels;
//		desc.arrayLayers = image->arrayLayers;
//		desc.samples = VK_SAMPLE_COUNT_1_BIT;
//		desc.tiling = VK_IMAGE_TILING_OPTIMAL;
//		desc.usage = usage;
//		desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
//		desc.queueFamilyIndexCount = 0;
//		desc.pQueueFamilyIndices = NULL;
//		desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//
//		VK_CHECK(vkCreateImage(vk.device, &desc, NULL, &image->handle), "failed to create Image!");
//		VK_CreateImageMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &image->handle, &image->memory);
//	}
//
//	// create image view
//	{
//		VkImageViewCreateInfo desc = {};
//		desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
//		desc.pNext = NULL;
//		desc.flags = 0;
//		desc.image = image->handle;
//		desc.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
//		desc.format = format;
//		desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
//		desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
//		desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
//		desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
//		desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//		desc.subresourceRange.baseMipLevel = 0;
//		desc.subresourceRange.levelCount = image->mipLevels;//VK_REMAINING_MIP_LEVELS;
//		desc.subresourceRange.baseArrayLayer = 0;
//		desc.subresourceRange.layerCount = image->arrayLayers;
//		VK_CHECK(vkCreateImageView(vk.device, &desc, NULL, &image->view), "failed to create Image View!");
//	}
//}
//
//
//void VK_TransitionImage(vkimage_t* image, VkImageLayout oldLayout, VkImageLayout newLayout)
//{
//	VkCommandBuffer commandBuffer;
//	VK_BeginSingleTimeCommands(&commandBuffer);
//
//	VkImageMemoryBarrier barrier = {};
//	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//	barrier.subresourceRange.baseMipLevel = 0;
//	barrier.subresourceRange.levelCount = 1;
//	barrier.subresourceRange.baseArrayLayer = 0;
//	barrier.subresourceRange.layerCount = 1;
//	barrier.subresourceRange.levelCount = image->mipLevels;
//
//	
//	// Source layouts (old)
//	// Source access mask controls actions that have to be finished on the old layout
//	// before it will be transitioned to the new layout
//	switch (oldLayout)
//	{
//		case VK_IMAGE_LAYOUT_UNDEFINED:
//			// Image layout is undefined (or does not matter)
//			// Only valid as initial layout
//			// No flags required, listed only for completeness
//			barrier.srcAccessMask = 0;
//			break;
//
//		case VK_IMAGE_LAYOUT_PREINITIALIZED:
//			// Image is preinitialized
//			// Only valid as initial layout for linear images, preserves memory contents
//			// Make sure host writes have been finished
//			barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
//			break;
//
//		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
//			// Image is a color attachment
//			// Make sure any writes to the color buffer have been finished
//			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
//			break;
//
//		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
//			// Image is a depth/stencil attachment
//			// Make sure any writes to the depth/stencil buffer have been finished
//			barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
//			break;
//
//		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
//			// Image is a transfer source 
//			// Make sure any reads from the image have been finished
//			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
//			break;
//
//		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
//			// Image is a transfer destination
//			// Make sure any writes to the image have been finished
//			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//			break;
//
//		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
//			// Image is read by a shader
//			// Make sure any shader reads from the image have been finished
//			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
//			break;
//		default:
//			// Other source layouts aren't handled (yet)
//			break;
//	}
//
//	// Target layouts (new)
//	// Destination access mask controls the dependency for the new image layout
//	switch (newLayout)
//	{
//		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
//			// Image will be used as a transfer destination
//			// Make sure any writes to the image have been finished
//			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//			break;
//
//		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
//			// Image will be used as a transfer source
//			// Make sure any reads from the image have been finished
//			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
//			break;
//
//		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
//			// Image will be used as a color attachment
//			// Make sure any writes to the color buffer have been finished
//			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
//			break;
//
//		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
//			// Image layout will be used as a depth/stencil attachment
//			// Make sure any writes to depth/stencil buffer have been finished
//			barrier.dstAccessMask = barrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
//			break;
//
//		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
//			// Image will be read in a shader (sampler, input attachment)
//			// Make sure any writes to the image have been finished
//			if (barrier.srcAccessMask == 0)
//			{
//				barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
//			}
//			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
//			break;
//		default:
//			// Other source layouts aren't handled (yet)
//			break;
//	}
//
//	barrier.oldLayout = oldLayout;
//	barrier.newLayout = newLayout;
//	barrier.image = image->handle;
//
//	vkCmdPipelineBarrier(commandBuffer,
//		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
//		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
//		0, 0, NULL, 0, NULL,
//		1, &barrier);
//
//	VK_EndSingleTimeCommands(&commandBuffer);
//}
//
//static void VK_CopyBufferToImage(vkimage_t* image, uint32_t width, uint32_t height, VkBuffer *buffer, uint32_t mipLevel, uint32_t arrayLayer)
//{
//	VkCommandBuffer commandBuffer;
//	VK_BeginSingleTimeCommands(&commandBuffer);
//
//	VkImageMemoryBarrier barrier = {};
//	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//	barrier.subresourceRange.baseArrayLayer = 0;
//	barrier.subresourceRange.layerCount = image->arrayLayers;
//	barrier.subresourceRange.baseMipLevel = 0;
//	barrier.subresourceRange.levelCount = image->mipLevels;
//
//	// transition to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
//	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//	barrier.srcAccessMask = 0;
//	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//	barrier.image = image->handle;
//
//	vkCmdPipelineBarrier(commandBuffer,
//		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
//		VK_PIPELINE_STAGE_TRANSFER_BIT,
//		0, 0, NULL, 0, NULL,
//		1, &barrier);
//
//	// buffer to image
//
//	VkBufferImageCopy region = { 0 };
//	region.bufferOffset = 0;
//	region.bufferRowLength = 0;
//	region.bufferImageHeight = 0;
//	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//	region.imageSubresource.mipLevel = mipLevel;
//	region.imageSubresource.baseArrayLayer = arrayLayer;
//	region.imageSubresource.layerCount = 1;
//	region.imageOffset = VkOffset3D{ 0, 0, 0 };
//    region.imageExtent = VkExtent3D{ width, height, 1 };
//
//	vkCmdCopyBufferToImage(commandBuffer, *buffer, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
//
//	// transition to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
//
//	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
//	barrier.image = image->handle;
//	vkCmdPipelineBarrier(commandBuffer,
//		VK_PIPELINE_STAGE_TRANSFER_BIT,
//		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
//		0, 0, NULL, 0, NULL,
//		1, &barrier);
//
//	VK_EndSingleTimeCommands(&commandBuffer);
//}
//
//void VK_DestroyImage(vkimage_t* image){
//	VK_CHECK(vkQueueWaitIdle(vk.graphicsQueue), "failed to wait for Queue execution!");
//
//    if(image->handle != NULL) {
//        vkDestroyImage(vk.device, image->handle, NULL);
//        image->handle = VK_NULL_HANDLE;
//    }
//    if(image->view != NULL) {
//        vkDestroyImageView(vk.device, image->view, NULL);
//        image->view = VK_NULL_HANDLE;
//    }
//    if(image->sampler != NULL) {
//        vkDestroySampler(vk.device, image->sampler, NULL);
//        image->sampler = VK_NULL_HANDLE;
//    }
//	if (image->memory != NULL) {
//		vkFreeMemory(vk.device, image->memory, NULL);
//		image->memory = VK_NULL_HANDLE;
//	}
//    
//    //VK_DestroyDescriptor(&image->descriptor_set);
//
//	memset(image, 0, sizeof(vkimage_t));
//}
//
//
//void VK_ReadDepthPixel(int x, int y, float* buffer) {}
//
//void VK_CopyImageToSwapchain(vkimage_t *image) {
//
//	VkCommandBuffer commandBuffer = vk.swapchain.commandBuffers[vk.swapchain.currentImage];
//
//	VkImageMemoryBarrier barrier = {};
//	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//	barrier.subresourceRange.baseMipLevel = 0;
//	barrier.subresourceRange.levelCount = 1;
//	barrier.subresourceRange.baseArrayLayer = 0;
//	barrier.subresourceRange.layerCount = 1;
//	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//	barrier.image = vk.swapchain.images[vk.swapchain.currentImage];
//	vkCmdPipelineBarrier(commandBuffer,
//		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
//		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
//		0, 0, NULL, 0, NULL,
//		1, &barrier);
//
//	barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
//	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
//	barrier.image = image->handle;
//	vkCmdPipelineBarrier(commandBuffer,
//		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
//		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
//		0, 0, NULL, 0, NULL,
//		1, &barrier);
//
//	VkImageCopy copyRegion = { 0 };
//	copyRegion.srcSubresource = VkImageSubresourceLayers{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
//	copyRegion.srcOffset = VkOffset3D{ 0, 0, 0 };
//	copyRegion.dstSubresource = VkImageSubresourceLayers{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
//	copyRegion.dstOffset = VkOffset3D{ 0, 0, 0 };
//	copyRegion.extent = VkExtent3D{ vk.swapchain.extent.width, vk.swapchain.extent.height, 1 };
//	vkCmdCopyImage(commandBuffer, image->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vk.swapchain.images[vk.swapchain.currentImage], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
//
//	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//	barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
//	barrier.image = vk.swapchain.images[vk.swapchain.currentImage];
//	vkCmdPipelineBarrier(commandBuffer,
//		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
//		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
//		0, 0, NULL, 0, NULL,
//		1, &barrier);
//
//	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
//	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
//	barrier.image = image->handle;
//	vkCmdPipelineBarrier(commandBuffer,
//		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
//		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
//		0, 0, NULL, 0, NULL,
//		1, &barrier);
//}
//

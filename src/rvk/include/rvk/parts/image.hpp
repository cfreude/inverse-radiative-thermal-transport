#pragma once
#include <rvk/parts/rvk_public.hpp>
#include <rvk/parts/sampler_config.hpp>

RVK_BEGIN_NAMESPACE
class LogicalDevice;
class Buffer;
class CommandBuffer;
class SingleTimeCommand;
class Image {
public:

	// helper for image creation {usage_flags}, but raw flags can also be used
	enum Use {
		// use to sample from the image
		SAMPLED = VK_IMAGE_USAGE_SAMPLED_BIT,
		// use as a storage image in a descriptor set
		STORAGE = VK_IMAGE_USAGE_STORAGE_BIT,
		// transfer data from or to the image
		UPLOAD = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		DOWNLOAD = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		COLOR_ATTACHMENT = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		DEPTH_STENCIL_ATTACHMENT = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
	};

													Image(LogicalDevice* aDevice);
													~Image();

	bool											isInit() const;
	VkImage											getHandle() const;
	VkSampler										getSampler() const;
	VkImageLayout									getLayout() const;
	VkExtent3D										getExtent() const;
	uint32_t										getArrayLayerCount() const;
	uint32_t										getMipMapCount() const;

	void											createImage2D(uint32_t aWidth, uint32_t aHeight, VkFormat aFormat,
	                                                              VkImageUsageFlags aUsage, uint32_t aMipLevels = 1, uint32_t aArrayLayers = 1);
													// based on the given config, search the static hash map in Sampler if it already exists, else create it
	void											setSampler(SamplerConfig aSamplerConfig);
	void											setSampler(VkSampler aSampler);
	void											destroy();

													// cmds
	//void											CMD_CopyImageToImage(VkCommandBuffer cb, VkRect2D extent, const Image* src_image, uint32_t mipLevel = 0, uint32_t arrayLayer = 0) const;
													// new cmds
	void											CMD_ClearColor(const CommandBuffer* aCmdBuffer, float aR, float aG, float aB, float aA) const;
	void											CMD_TransitionImage(const CommandBuffer* aCmdBuffer, VkImageLayout aNewLayout);
	void											CMD_CopyBufferToImage(const CommandBuffer* aCmdBuffer, const Buffer* aSrcBuffer, uint32_t aMipLevel = 0, uint32_t aArrayLayer = 0, VkExtent3D aImageExtent = {}, VkOffset3D aImageOffset = {}, VkDeviceSize aBufferOffset = 0) const;
	void											CMD_CopyImageToBuffer(const CommandBuffer* aCmdBuffer, const Buffer* aDstBuffer, uint32_t aMipLevel = 0, uint32_t aArrayLayer = 0, VkExtent3D aImageExtent = {}, VkOffset3D aImageOffset = {}, VkDeviceSize aBufferOffset = 0) const;
	//void											CMD_CopyImageToImage(const CommandBuffer* aCmdBuffer, const Image* aSrcImage);
	void											CMD_GenerateMipmaps(const CommandBuffer* aCmdBuffer) const;
	void											CMD_BlitImage(const CommandBuffer* aCmdBuffer, const rvk::Image* aSrcImage, VkFilter aFilter) const;
	void											CMD_ImageBarrier(const CommandBuffer* aCmdBuffer, VkPipelineStageFlags aSrcStage, VkPipelineStageFlags aDstStage, VkAccessFlags aSrcAccess, VkAccessFlags aDstAccess, VkDependencyFlags aDependency) const;

													// HELPER FUNCTIONS
													// upload image data through a staging buffer
	void											STC_UploadData2D(SingleTimeCommand* aStc, uint32_t aWidth, uint32_t aHeight, uint32_t aBytesPerPixel,
														const void* aDataIn, uint32_t aMipLevel = 0, uint32_t aArrayLayer = 0);
	void											STC_DownloadData2D(SingleTimeCommand* aStc, uint32_t aWidth, uint32_t aHeight, uint32_t aBytesPerPixel,
														void* aDataOut, uint32_t aArrayLayer = 0);
	void											STC_GenerateMipmaps(SingleTimeCommand* aStc) const;
	void											STC_TransitionImage(SingleTimeCommand* aStc, VkImageLayout aNewLayout);
private:
	friend class									Descriptor;
	friend class									Framebuffer;
	friend class									Swapchain;

	void											createImage(VkImageType aImageType, VkImageUsageFlags aUsage);
	void											createImageView(VkImageViewType aViewType);

													// copy buffer to the image at the set mipLevel and arrayLayer
	void											STC_CopyBufferToImage(SingleTimeCommand* aStc, const Buffer* aBuffer, uint32_t aMipLevel = 0, uint32_t aArrayLayer = 0, VkExtent3D aImageExtent = {}, VkOffset3D aImageOffset = {}, VkDeviceSize aBufferOffset = 0);
	void											STC_CopyImageToBuffer(SingleTimeCommand* aStc, const Buffer* aBuffer, uint32_t aMipLevel = 0, uint32_t ArrayLayer = 0, VkExtent3D aImageExtent = {}, VkOffset3D aImageOffset = {}, VkDeviceSize aBufferOffset = 0);


	void											createImageMemory();

	LogicalDevice*									mDevice;

	VkExtent3D										mExtent;
	VkFormat										mFormat;
	VkMemoryPropertyFlags							mProperties;
	VkImageAspectFlags								mAspect;
	uint32_t										mMipLevels;
	uint32_t										mArrayLayers;
	VkImageLayout                                   mLayout;
	VkImageUsageFlags								mUsageFlags;

	VkImage											mImage;
	VkImageView										mImageView;
	VkSampler										mSampler;
	VkDeviceMemory									mMemory;
};
RVK_END_NAMESPACE
#include <rvk/rvk.hpp>
#include <rvk/parts/command_buffer.hpp>
#include "parts/rvk_private.hpp"
RVK_USE_NAMESPACE

// Download the current swapchain content and save it into an array of size:
// vk.swapchain.extent.width * vk.swapchain.extent.height * 3|4 (rgb|rgba)
uint8_t* rvk::swapchain::readPixelsScreen(SingleTimeCommand* stc, Swapchain* sc, const bool alpha) {
	LogicalDevice* device = sc->getDevice();
	device->waitIdle();

	VkExtent2D extent = sc->getExtent();
	uint8_t* buffer = new uint8_t[extent.width * extent.height * (alpha ? 4 : 3)];

	// Create image to copy swapchain content on host
	VkImageCreateInfo desc = {};
	desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	desc.pNext = VK_NULL_HANDLE;
	desc.flags = 0;
	desc.imageType = VK_IMAGE_TYPE_2D;
	desc.format = VK_FORMAT_R8G8B8A8_UNORM;
	desc.extent.width = extent.width;
	desc.extent.height = extent.height;
	desc.extent.depth = 1;
	desc.mipLevels = 1;
	desc.arrayLayers = 1;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.tiling = VK_IMAGE_TILING_LINEAR;
	desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = VK_NULL_HANDLE;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkImage image;
	VK_CHECK(device->vk.CreateImage(device->getHandle(), &desc, NULL, &image), "failed to create image");

	VkMemoryRequirements mRequirements;
	device->vk.GetImageMemoryRequirements(device->getHandle(), image, &mRequirements);

	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = VK_NULL_HANDLE;
	alloc_info.allocationSize = mRequirements.size;
	alloc_info.memoryTypeIndex = device->findMemoryTypeIndex(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mRequirements.memoryTypeBits);
	ASSERT(alloc_info.memoryTypeIndex != -1, "could not find suitable Memory");

	VkDeviceMemory memory;
	VK_CHECK(device->vk.AllocateMemory(device->getHandle(), &alloc_info, NULL, &memory), "could not allocate Image Memory");
	VK_CHECK(device->vk.BindImageMemory(device->getHandle(), image, memory, 0), "could not bind Image Memory");

	stc->begin();

	// transition dst image
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;

	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.image = image;

	device->vkCmd.PipelineBarrier(stc->buffer()->getHandle(),
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE,
		1, &barrier);

	// transition swap chain image
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;

	VkImageLayout originalLayout = sc->getCurrentImage()->getLayout();
	sc->getCurrentImage()->CMD_TransitionImage(stc->buffer(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	VkImageCopy region = {};
	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.srcSubresource.mipLevel = 0;
	region.srcSubresource.baseArrayLayer = 0;
	region.srcSubresource.layerCount = 1;
	region.dstSubresource = region.srcSubresource;
	region.dstOffset = region.srcOffset;
	region.extent.width = extent.width;
	region.extent.height = extent.height;
	region.extent.depth = 1;

	device->vkCmd.CopyImage(stc->buffer()->getHandle(), sc->getCurrentImage()->getHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		image, VK_IMAGE_LAYOUT_GENERAL, 1, &region);

	sc->getCurrentImage()->CMD_TransitionImage(stc->buffer(), originalLayout);
	stc->end();

	// Copy data from destination image to memory buffer.
	VkImageSubresource subresource = {};
	subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource.mipLevel = 0;
	subresource.arrayLayer = 0;
	VkSubresourceLayout layout = {};
	device->vk.GetImageSubresourceLayout(device->getHandle(), image, &subresource, &layout);

	VkFormat format = sc->getSurfaceFormat().format;
	bool swizzleComponents = (format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SNORM);

	uint8_t* data;
	VK_CHECK(device->vk.MapMemory(device->getHandle(), memory, 0, VK_WHOLE_SIZE, 0, (void**)&data), "failed to map memory");

	uint8_t* pBuffer = buffer;
	for (uint32_t y = 0; y < extent.height; y++) {
		for (uint32_t x = 0; x < extent.width; x++) {
			uint8_t pixel[4];
			memcpy(&pixel, &data[x * 4], 4);

			// copy pixel to buffer
			pBuffer[0] = pixel[0];
			pBuffer[1] = pixel[1];
			pBuffer[2] = pixel[2];
			if (alpha) pBuffer[3] = pixel[3];
			if (swizzleComponents) {
				float temp = pBuffer[0];
				pBuffer[0] = pixel[2];
				pBuffer[2] = temp;
			}

			// offset buffer by 3 (RGB) or 4 (RGBA)
			pBuffer += alpha ? 4 : 3;
		}
		data += layout.rowPitch;
	}

	device->vk.DestroyImage(device->getHandle(), image, VK_NULL_HANDLE);
	device->vk.FreeMemory(device->getHandle(), memory, VK_NULL_HANDLE);
	return buffer;
}

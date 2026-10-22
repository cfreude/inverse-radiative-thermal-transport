#pragma once
#include <rvk/parts/rvk_public.hpp>

RVK_BEGIN_NAMESPACE
class Image;
class Buffer;
class TopLevelAS;
class LogicalDevice;
class Descriptor {
public:
	enum BindingFlag {
		PARTIALLY_BOUND = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT,
		VARIABLE_DESCRIPTOR_COUNT = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT
	};

													Descriptor(LogicalDevice* aDevice);
													~Descriptor();

													// reserve memory for the amount of bindings to add
	void											reserve(int aReserve);

													// add attachments to the descriptor
													// use count > 1 to add multiple data to a single binding
	void											addImageSampler(uint32_t aBinding, VkShaderStageFlags aStages, uint32_t aCount = 1);
	void											addStorageImage(uint32_t aBinding, VkShaderStageFlags aStages, uint32_t aCount = 1);
	void											addUniformBuffer(uint32_t aBinding, VkShaderStageFlags aStages, uint32_t aCount = 1);
	void											addStorageBuffer(uint32_t aBinding, VkShaderStageFlags aStages, uint32_t aCount = 1);
	void											addAccelerationStructureKHR(uint32_t aBinding, VkShaderStageFlags aStages, uint32_t aCount = 1);
													// set the data for a given binding
													// use position to set data in the data array of a given binding
	void											setImage(uint32_t aBinding, const Image* aImage, uint32_t aPosition = 0);
	void											setBuffer(uint32_t aBinding, const Buffer* aBuffer, uint32_t aPosition = 0);
	void											setAccelerationStructureKHR(uint32_t aBinding, const TopLevelAS* aTlas, uint32_t aPosition = 0);
													// this function can be used when a binding has more than one attachment and not all are
													// populated, e.g. binding with 100 images; only 50 are currently populated, but there 
													// will be more added in the future
	void											setUpdateSize(uint32_t aBinding, uint32_t aUpdateSize);

													// bits from VkDescriptorBindingFlagBitsEXT
	void											setBindingFlags(uint32_t aBinding, VkDescriptorBindingFlags aFlags);

													// build the descriptor
													// use with_update to specify if all the needed attachment data is present and the descriptor can be
													// finished and updated with this data or there is still data missing and the descriptor should be build without
													// updating, in the case of with_update=false, update() must be called at a later stage when all the data is present
	void											finish(bool aWithUpdate = true);
	void											update() const;

	VkDescriptorSet									getHandle() const;

	void											destroy();
private:
	friend class									Pipeline;
	friend class									RPipeline;
	friend class									CRipeline;
	friend class									RTPipeline;

	void											createDescriptorSetLayout();
	void											createDescriptorPool();
	void											createDescriptorSet();

	// helper functions for adding bindings to the descriptor
	void											pushImage(uint32_t aBinding, VkShaderStageFlags aStages, uint32_t aCount, VkDescriptorType aType, VkImageLayout aLayout);
	void											pushBuffer(uint32_t aBinding, VkShaderStageFlags aStages, uint32_t aCount, VkDescriptorType aType);

	struct bindings_s {
		VkDescriptorSetLayoutBinding					binding = {};
		std::vector<VkDescriptorImageInfo>				descImageInfos = {};
		std::vector<VkDescriptorBufferInfo>				descBufferInfos = {};
		// acceleration structure
		VkWriteDescriptorSetAccelerationStructureKHR	descASInfo = {};
		std::vector<VkAccelerationStructureKHR>			accelerationStructures = {};
		uint32_t										updateSize = 0;
	};
	std::vector<bindings_s>							mBindings;
	std::vector<VkDescriptorBindingFlags>			mBindingFlags;

	LogicalDevice*									mDevice;
	VkDescriptorSetLayout							mLayout;
	VkDescriptorPool								mPool;
	VkDescriptorSet									mSet;
};

RVK_END_NAMESPACE

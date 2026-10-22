#pragma once
#include <rvk/parts/rvk_public.hpp>
#include <rvk/parts/acceleration_structure.hpp>

RVK_BEGIN_NAMESPACE
class BottomLevelAS;
class LogicalDevice;
class Buffer;
class CommandBuffer;
// temporal representation of one instance of a bottom acceleration structure
// once it is added to a top level acceleration structure it can be deleted 
class ASInstance {
public:
													ASInstance(const BottomLevelAS* aBottom);
													~ASInstance() = default;
													// set additional optional attributes
	void											setCustomIndex(uint32_t aIndex);
	void											setMask(uint32_t aMask);
	void											setSBTRecordOffset(uint32_t aOffset);
	void											setFlags(VkGeometryInstanceFlagsKHR aFlags);
	void											setTransform(const float* aMat3X4);

												private:
	friend class									TopLevelAS;
	VkAccelerationStructureInstanceKHR				mInstance;
};


class TopLevelAS final : public AccelerationStructure {
public:
													TopLevelAS(LogicalDevice* aDevice);
													~TopLevelAS();
													// reserve the amount of instances to add
	void											reserve(uint32_t aReserve);
													// optional, set a custom scratch buffer, only requiered during build
	void											setInstanceBuffer(rvk::Buffer* aBuffer, uint32_t aOffset = 0);

													// add one instance to this top as
	void											addInstance(ASInstance const& aBottomInstance);
	void											replaceInstance(uint32_t aIndex, ASInstance const& aBottomInstance);
													// (optional) explicitly query required buffer sizes
	void											preprepare(VkBuildAccelerationStructureFlagsKHR aBuildFlags);
													// prepare this as for building
	void											prepare(VkBuildAccelerationStructureFlagsKHR aBuildFlags, VkGeometryFlagsKHR aInstanceFlags = 0);
													// get the amount of instances added
	uint32_t										size() const;
													// get the size requiered for the instance buffer
	uint32_t										getInstanceBufferSize() const;
													// clear all instances from this tlas
	void											clear();
													// destroy temporary buffers created for building, when not explicitly set
	void											destroyTempBuffers() override;
													// destroy this tas
	void											destroy();

													// CMD
	void											CMD_Build(const CommandBuffer* aCmdBuffer, TopLevelAS* aSrcAs = nullptr);
private:
	friend class									Descriptor;


	VkGeometryFlagsKHR								mInstanceFlags;
	std::vector<VkAccelerationStructureInstanceKHR> mInstanceList;

	buffer_s										mInstanceBuffer;
};
RVK_END_NAMESPACE
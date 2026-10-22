#pragma once
#include <rvk/parts/rvk_public.hpp>

RVK_BEGIN_NAMESPACE
class Buffer;
class LogicalDevice;
// AS update info:
// the content of an as which is updated must be the same as it previously was
// for tlas this means, the #instance must be the same as its source as
// for blas this means, the #geometry must be the same as its source as and
// the triangle count must also be the same
class AccelerationStructure {
public:

	// optional, set a custom scratch buffer, only required during build
	void											setScratchBuffer(Buffer* aBuffer, uint32_t aOffset = 0);
	void											setASBuffer(Buffer* aBuffer, uint32_t aOffset = 0);

	uint32_t										getASSize() const;
	uint32_t										getBuildScratchSize() const;
	uint32_t										getUpdateScratchSize() const;
	bool											isBuild() const;

	VkDeviceAddress									getAccelerationStructureDeviceAddress() const;

	// clean all not needed buffers
	// if this as was build with a single time command which was already executed
	// use this command to destroy the scratch/instance buffers
	// do not use this command when a global scratch buffer for instance is set
	virtual void									destroyTempBuffers() = 0;
protected:
													AccelerationStructure(LogicalDevice* aDevice);
													~AccelerationStructure() = default;
	struct buffer_s {
		rvk::Buffer* buffer = nullptr;
		uint32_t									offset = 0;
		bool										external = false;	// is this a buffer created just for this AS or an external one
	};
	// check if buffer is set or an internal buffer needs to be created
	// also check internal if size is large enough
	// returns false when buffer is not ok and a internal needs to be created
	static bool										checkBuffer(const buffer_s& aBuffer, uint32_t aReqSize);
	// deletes the buffer only if it is internal
	static void										deleteInternalBuffer(buffer_s& aBuffer);
	virtual void											destroy();

	LogicalDevice*									mDevice;
	bool											mValidSize;
	uint32_t										mAsSize;		// for buffer holding the as
	uint32_t										mBuildSize;		// for scratch buffer
	uint32_t										mUpdateSize;	// for scratch buffer

	buffer_s										mScratchBuffer;
	buffer_s										mAsBuffer;

	bool											mBuild;
	VkAccelerationStructureBuildGeometryInfoKHR		mBuildInfo;
	VkAccelerationStructureKHR						mAs;
};
RVK_END_NAMESPACE
#pragma once
#include <rvk/parts/rvk_public.hpp>
#include <rvk/parts/acceleration_structure.hpp>

RVK_BEGIN_NAMESPACE
class Buffer;
class CommandBuffer;
// temporal holds the information of one geometry used in a bottom acceleration struct
// used to add geometries to an bottom acceleration struct through bottomAS.addGeometry()
class ASTriangleGeometry {
public:
													ASTriangleGeometry();
													~ASTriangleGeometry();
													// offset describes the start of the data in bytes from the beginning of the corresponding buffer
													// vertices
	void											setVerticesFromDevice(VkFormat aFormat, uint32_t aStride, uint32_t aVertexCount, const rvk::Buffer* aVertexBuffer, uint32_t aOffset = 0);
													// indices
	void											setIndicesFromDevice(VkIndexType aType, uint32_t aIndexCount, const rvk::Buffer* aIndexBuffer, uint32_t aOffset = 0);
													// transform (of VkTransformMatrixKHR {float matrix[3][4];}
	void											setTransformFromDevice(BufferHandle aBufferHandle);

private:
	friend class									BottomLevelAS;
	VkAccelerationStructureGeometryTrianglesDataKHR mTriangleData;

	bool											mHasIndices;
	uint32_t										mTriangleCount;
	uint32_t										mIndexBufferOffset;
	uint32_t										mVertexBufferOffset;
	uint32_t										mTransformOffset;
};
class ASAABBGeometry {
public:
	typedef VkAabbPositionsKHR AABB_s;
	ASAABBGeometry();

	void											setAABBsFromDevice(const rvk::Buffer* aAabbBuffer, uint32_t aAabbCount = 1, uint32_t aStride = 8, uint32_t aOffset = 0);
private:
	friend class									BottomLevelAS;
	VkAccelerationStructureGeometryAabbsDataKHR		mAabbData;
	uint32_t										mAabbCount;
	uint32_t										mAabbOffset;
};

class BottomLevelAS final : public AccelerationStructure {
public:
													BottomLevelAS(LogicalDevice* aDevice);
													~BottomLevelAS();

													// reserve space for the amount of geometries this bottom as will contain
	void											reserve(uint32_t aReserve);
													// add geometry to this bottom level acceleration structure
	void											addGeometry(ASTriangleGeometry const& aTriangleGeometry, VkGeometryFlagsKHR aFlags);
	void											addGeometry(ASAABBGeometry const& aAabbGeometry, VkGeometryFlagsKHR aFlags);
													// (optional) explicitly query required buffer sizes
	void											preprepare(VkBuildAccelerationStructureFlagsKHR aBuildFlags);
													// prepare this as for building
	void											prepare(VkBuildAccelerationStructureFlagsKHR aBuildFlags);
													// get the amount of geometries added
	int												size() const;
													// clear all geometries from this blas
	void											clear();
													// destroy temporary buffers created for building (only do this once the build process is completed)
													// only necessary in cases where only internal buffers are used
	void											destroyTempBuffers() override;
													// destroy this bas
	void											destroy();

													// CMD
	void											CMD_Build(const CommandBuffer* aCmdBuffer, BottomLevelAS* aSrcAs = nullptr);
private:
	friend class									TopLevelAS;
	friend class									ASInstance;


	std::vector<VkAccelerationStructureGeometryKHR>			mGeometry;		// overall geometry describtion
	std::vector<VkAccelerationStructureBuildRangeInfoKHR>	mRange;			// contains the offsets for the buffers
	std::vector<uint32_t>									mPrimitives;		// primitive count for each added geometry
};
RVK_END_NAMESPACE
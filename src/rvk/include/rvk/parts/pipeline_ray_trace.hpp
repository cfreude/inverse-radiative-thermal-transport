#pragma once
#include <rvk/parts/rvk_public.hpp>
#include <rvk/parts/pipeline.hpp>

RVK_BEGIN_NAMESPACE
class Buffer;
class RTShader;
class RTPipeline final : public Pipeline {
public:
													RTPipeline(LogicalDevice* aDevice, int aRecursionDepth = 5);
													~RTPipeline();
													// setup RT pipeline
	void											setShader(RTShader* aShader);
	void											destroy() override;

													// CMDs
	void											CMD_TraceRays(const CommandBuffer* aCmdBuffer, uint32_t aWidth, uint32_t aHeight = 1, uint32_t aDepth = 1, uint32_t aRgenOffset = 0) const;
	void											CMD_TraceRaysIndirect(const CommandBuffer* aCmdBuffer, const rvk::Buffer* aBuffer, uint32_t aRgenOffset = 0) const;
private:

	void											createShaderBindingTable(bool aFront);
	void											createPipeline(bool aFront) override;

	VkStridedDeviceAddressRegionKHR					mDeviceRegionRaygen;
	VkStridedDeviceAddressRegionKHR					mDeviceRegionMiss;
	VkStridedDeviceAddressRegionKHR					mDeviceRegionHit;
	VkStridedDeviceAddressRegionKHR					mDeviceRegionCallable;

	int												mRecursionDepth;
	rvk::RTShader*									mShader;
													// when the shader of this pipeline is reloaded
													// the front buffer is swapped with the back and a
													// new sbt buffer is created in the front for the new shaders
	rvk::Buffer*									mSbtBufferFront;
	rvk::Buffer*									mSbtBufferBack;
};
RVK_END_NAMESPACE
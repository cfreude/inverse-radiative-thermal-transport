#include <rvk/parts/pipeline_ray_trace.hpp>
#include <rvk/parts/command_buffer.hpp>
#include "rvk_private.hpp"
RVK_USE_NAMESPACE

RTPipeline::RTPipeline(LogicalDevice* aDevice, const int aRecursionDepth) : Pipeline(aDevice, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR), mDeviceRegionRaygen(), mDeviceRegionMiss(), mDeviceRegionHit(), mDeviceRegionCallable(),
                                                                            mRecursionDepth(aRecursionDepth), mShader(nullptr), mSbtBufferFront(nullptr), mSbtBufferBack(nullptr)
{}

rvk::RTPipeline::~RTPipeline()
{
    if (mSbtBufferFront) {
        delete mSbtBufferFront;
        mSbtBufferFront = nullptr;
    }
    if (mSbtBufferBack) {
        delete mSbtBufferBack;
        mSbtBufferBack = nullptr;
    }
    mShader = nullptr;
}
void rvk::RTPipeline::setShader(RTShader* const aShader)
{
    mShader = aShader;
    // add for reload, also remove duplicates
    aShader->linkPipeline(this);
}

void rvk::RTPipeline::destroy()
{
    Pipeline::destroy();
    if (mShader != nullptr) {
        mShader->unlinkPipeline(this);
        mShader = nullptr;
    }
    if (mSbtBufferFront) {
        delete mSbtBufferFront;
        mSbtBufferFront = nullptr;
    }
    if (mSbtBufferBack) {
        delete mSbtBufferBack;
        mSbtBufferBack = nullptr;
    }
}

void RTPipeline::CMD_TraceRays(const CommandBuffer* aCmdBuffer, const uint32_t aWidth, const uint32_t aHeight, const uint32_t aDepth,
                               const uint32_t aRgenOffset) const
{
    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rtp = mDevice->getPhysicalDevice()->getRayTracingPipelineProperties();
    // offset the rgen shader in the sbt
    const int handleSizeAlignment = rountUpToMultipleOf<int>(rtp.shaderGroupHandleSize, rtp.shaderGroupHandleAlignment);
    const int baseSizeAlignment = rountUpToMultipleOf<int>(handleSizeAlignment, rtp.shaderGroupBaseAlignment);
    VkStridedDeviceAddressRegionKHR deviceRegionRaygenStrided = mDeviceRegionRaygen;
    const uint32_t rgenHandleOffset = aRgenOffset * baseSizeAlignment;
    deviceRegionRaygenStrided.deviceAddress += rgenHandleOffset;
    ASSERT(rgenHandleOffset < deviceRegionRaygenStrided.size, "Offset must be within the range of rgen groups");
    deviceRegionRaygenStrided.size = baseSizeAlignment;

    mDevice->vkCmd.TraceRaysKHR(aCmdBuffer->getHandle(),
        &deviceRegionRaygenStrided,
        &mDeviceRegionMiss,
        &mDeviceRegionHit,
        &mDeviceRegionCallable,
        aWidth, aHeight, aDepth);
}

void RTPipeline::CMD_TraceRaysIndirect(const CommandBuffer* aCmdBuffer, const rvk::Buffer* aBuffer, const uint32_t aRgenOffset) const
{
    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rtp = mDevice->getPhysicalDevice()->getRayTracingPipelineProperties();
    // offset the rgen shader in the sbt
    const int handleSizeAlignment = rountUpToMultipleOf<int>(rtp.shaderGroupHandleSize, rtp.shaderGroupHandleAlignment);
    const int baseSizeAlignment = rountUpToMultipleOf<int>(handleSizeAlignment, rtp.shaderGroupBaseAlignment);
    VkStridedDeviceAddressRegionKHR deviceRegionRaygenStrided = mDeviceRegionRaygen;
    const uint32_t rgenHandleOffset = aRgenOffset * baseSizeAlignment;
    deviceRegionRaygenStrided.deviceAddress += aRgenOffset * rgenHandleOffset;
    ASSERT(rgenHandleOffset < deviceRegionRaygenStrided.size, "Offset must be within the range of rgen groups");
    deviceRegionRaygenStrided.size = baseSizeAlignment;

    mDevice->vkCmd.TraceRaysIndirectKHR(aCmdBuffer->getHandle(),
        &deviceRegionRaygenStrided,
        &mDeviceRegionMiss,
        &mDeviceRegionHit,
        &mDeviceRegionCallable,
        aBuffer->getBufferDeviceAddress());
}

/**
* PRIVATE
**/
void rvk::RTPipeline::createShaderBindingTable(const bool aFront) {
    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rtp = mDevice->getPhysicalDevice()->getRayTracingPipelineProperties();
    const uint32_t handleSize = rtp.shaderGroupHandleSize;
    const uint32_t handleSizeAligned = rountUpToMultipleOf<uint32_t>(rtp.shaderGroupHandleSize, rtp.shaderGroupHandleAlignment);
    const uint32_t groupCount = static_cast<uint32_t>(mShader->getShaderGroupCreateInfo().size());
    const uint32_t sbtSize = groupCount * handleSizeAligned;
    const uint32_t baseSizeAlignment = rountUpToMultipleOf<uint32_t>(handleSizeAligned, rtp.shaderGroupBaseAlignment);

    // contains data for each group with a size of rtp.shaderGroupHandleSize
    // the stride is also of size rtp.shaderGroupHandleSize
    std::vector<uint8_t> shaderHandleStorage(sbtSize);
    VK_CHECK(mDevice->vk.GetRayTracingShaderGroupHandlesKHR(mDevice->getHandle(), aFront ? mHandleFront : mHandleBack, 0, groupCount, sbtSize, shaderHandleStorage.data()), " failed to get ray tracing group handles");

    const RTShader::sbtInfo rgen = mShader->getRgenSBTInfo();
    const RTShader::sbtInfo miss = mShader->getMissSBTInfo();
    const RTShader::sbtInfo hit = mShader->getHitSBTInfo();
    const RTShader::sbtInfo call = mShader->getCallableSBTInfo();

    // receive total buffer size with rtp.shaderGroupBaseAlignment applied
    uint32_t sbtBufferSize = 0;
    if (rgen.baseAlignedOffset != -1) {
        sbtBufferSize += rountUpToMultipleOf<uint32_t>(rgen.handleSize, rtp.shaderGroupBaseAlignment);
    }
    if (miss.baseAlignedOffset != -1) {
        sbtBufferSize += rountUpToMultipleOf<uint32_t>(miss.handleSize, rtp.shaderGroupBaseAlignment);
    }
    if (hit.baseAlignedOffset != -1) {
        sbtBufferSize += rountUpToMultipleOf<uint32_t>(hit.handleSize, rtp.shaderGroupBaseAlignment);
    }
    if (call.baseAlignedOffset != -1) {
        sbtBufferSize += rountUpToMultipleOf<uint32_t>(call.handleSize, rtp.shaderGroupBaseAlignment);
    }

    // save data to the sbt buffer with correct base offset of rtp.shaderGroupBaseAlignment for each type
    // each entry of one type has size rtp.shaderGroupHandleSize aligned with rtp.shaderGroupHandleAlignment
    // and a stride of this alligned size
    // for the rgen shader every entry must be rtp.shaderGroupBaseAlignment in order to offset the beginning of the buffer 
    // with an offset of rtp.shaderGroupBaseAlignment
    //sbt_buffer_front->createShaderBindingTableBuffer(sbtBufferSize);
    mSbtBufferFront->create(rvk::Buffer::Use::SBT, sbtBufferSize, rvk::Buffer::Location::HOST_COHERENT);
    mSbtBufferFront->mapBuffer();
    if (rgen.baseAlignedOffset != -1) {
        uint8_t* p = (uint8_t*)mSbtBufferFront->getMemoryPointer();
        p += rgen.baseAlignedOffset;
        for (int i = 0; i < rgen.count; i++) {
            std::memcpy(p, &shaderHandleStorage[0] + handleSizeAligned * (rgen.countOffset + i), (size_t)(handleSizeAligned));
            p += baseSizeAlignment;
        }
    }
    if (miss.baseAlignedOffset != -1)  mSbtBufferFront->STC_UploadData(nullptr, &shaderHandleStorage[0] + handleSizeAligned * miss.countOffset, miss.handleSize, miss.baseAlignedOffset);
    if (hit.baseAlignedOffset != -1)  mSbtBufferFront->STC_UploadData(nullptr, &shaderHandleStorage[0] + handleSizeAligned * hit.countOffset, hit.handleSize, hit.baseAlignedOffset);
    if (call.baseAlignedOffset != -1)  mSbtBufferFront->STC_UploadData(nullptr, &shaderHandleStorage[0] + handleSizeAligned * call.countOffset, call.handleSize, call.baseAlignedOffset);
    mSbtBufferFront->unmapBuffer();

    // create the device region struct for later use with vkCmdTraceRaysKHR
    const VkDeviceAddress deviceAddress = mSbtBufferFront->getBufferDeviceAddress();
    if (rgen.baseAlignedOffset != -1) {
        mDeviceRegionRaygen.deviceAddress = deviceAddress + rgen.baseAlignedOffset;
        mDeviceRegionRaygen.size = rgen.handleSize;
        mDeviceRegionRaygen.stride = baseSizeAlignment;
    }
    if (miss.baseAlignedOffset != -1) {
        mDeviceRegionMiss.deviceAddress = deviceAddress + miss.baseAlignedOffset;
        mDeviceRegionMiss.size = miss.handleSize;
        mDeviceRegionMiss.stride = handleSizeAligned;
    }
    if (hit.baseAlignedOffset != -1) {
        mDeviceRegionHit.deviceAddress = deviceAddress + hit.baseAlignedOffset;
        mDeviceRegionHit.size = hit.handleSize;
        mDeviceRegionHit.stride = handleSizeAligned;
    }
    if (call.baseAlignedOffset != -1) {
        mDeviceRegionCallable.deviceAddress = deviceAddress + call.baseAlignedOffset;
        mDeviceRegionCallable.size = call.handleSize;
        mDeviceRegionCallable.stride = handleSizeAligned;
    }
}

void rvk::RTPipeline::createPipeline(const bool aFront)
{
    VkRayTracingPipelineCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    createInfo.stageCount = static_cast<uint32_t>(mShader->getShaderStageCreateInfo().size());
    createInfo.pStages = mShader->getShaderStageCreateInfo().data();
    createInfo.groupCount = static_cast<uint32_t>(mShader->getShaderGroupCreateInfo().size());
    createInfo.pGroups = mShader->getShaderGroupCreateInfo().data();
    createInfo.maxPipelineRayRecursionDepth = mRecursionDepth;
    createInfo.layout = mLayout;
    VK_CHECK(mDevice->vk.CreateRayTracingPipelinesKHR(mDevice->getHandle(), VK_NULL_HANDLE, mCache, 1, &createInfo, VK_NULL_HANDLE, aFront ? &mHandleFront : &mHandleBack), " failed to create Ray Tracing Pipeline");

    // delete sbt buffer if not created and create new one
    delete mSbtBufferBack;
    mSbtBufferBack = mSbtBufferFront;
    mSbtBufferFront = new rvk::Buffer(mDevice);
    createShaderBindingTable(aFront);
}



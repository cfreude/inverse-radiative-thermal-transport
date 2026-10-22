#pragma once
#include <rvk/parts/rvk_public.hpp>

#include <mutex>

RVK_BEGIN_NAMESPACE
class Descriptor;
class CommandBuffer;
class LogicalDevice;
class Pipeline {
public:
													// the input order defines the set index
	void											addDescriptorSet(const std::vector<Descriptor*>& aDescriptors);
	void											addPushConstant(VkShaderStageFlags aStageFlags, uint32_t aOffset, uint32_t aSize);
	void											finish();
													// delete front pipeline and use back as new front
	void											rebuildPipeline();
	virtual void									destroy();

													// CMDs
	void											CMD_BindPipeline(const CommandBuffer* aCmdBuffer);
													// the input order defines the set index
	void											CMD_BindDescriptorSets(const CommandBuffer* aCmdBuffer, const std::vector<Descriptor*>& aDescriptors, uint32_t aFirstSetIndex = 0) const;
	void											CMD_SetPushConstant(const CommandBuffer* aCmdBuffer, VkShaderStageFlags aStage, uint32_t aOffset, uint32_t aSize, const void* aData) const;

	static bool										pcache;
	static std::string								cache_dir;
protected:
													Pipeline(LogicalDevice* aDevice, VkPipelineBindPoint aPipelineBindPoint);
													~Pipeline();
													Pipeline(const Pipeline& aOther);
	Pipeline&										operator=(const Pipeline& aOther);

	void											createPipelineCache() const;
	void											createPipelineLayout();
													// create the different pipelines (rasterizer, compute, ray tracing)
	virtual void									createPipeline(bool aFront) = 0;

	LogicalDevice*									mDevice;
	const VkPipelineBindPoint						mPipelineBindPoint;

	static VkPipelineCache							mCache;
	VkPipelineLayout								mLayout;
	VkPipeline										mHandleFront;
	VkPipeline										mHandleBack;
	std::deque<VkPipeline>							mRipPipelines;	// old pipelines
													// since we create the new pipeline in another thread, handle_back can be != NULL even when the pipeline is not completed yet
													// back_handle_write_done is set after handle_back is completely created
	std::mutex										mBackHandleWriteMutex;

	std::vector<Descriptor*>						mDescriptors;
	std::vector<VkPushConstantRange>				mPushConstantRange;
};

RVK_END_NAMESPACE
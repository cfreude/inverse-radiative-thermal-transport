#pragma once
#include <rvk/parts/rvk_public.hpp>
#include <rvk/parts/shader.hpp>

RVK_BEGIN_NAMESPACE
class RTShader final : public Shader {
public:
	struct sbtInfo {
		sbtInfo() : handleSize(0), baseAlignedOffset(-1), countOffset(-1), count(0) {}
		int											handleSize;
		int											baseAlignedOffset;
		int											countOffset;
		int											count;
	};


													RTShader(LogicalDevice* aDevice);

													// reserve memory for the amount of groups to add
	void											reserveGroups(int aCount);
	// add shader groups to the shader binding table with the order of execution
													// automatically finds the correct shader index from the name
													// for ray generation, miss or callable shader
	void											addGeneralShaderGroup(int aGeneralShaderIndex);
	void											addGeneralShaderGroup(const std::string& aGeneralShader);
													// for closest hit or any hit shader
													// for any empty string that is passed, no shader is used (VK_SHADER_UNUSED_KHR)
	void											addHitShaderGroup(int aClosestHitShaderIndex, int aAnyHitShaderIndex = -1);
	void											addHitShaderGroup(const std::string& aClosestHitShader, const std::string& aAnyHitShader = "");
													// for closest hit, any hit or intersection shader
													// for any empty string that is passed, no shader is used (VK_SHADER_UNUSED_KHR)
	void											addProceduralShaderGroup(const std::string& aClosestHitShader, const std::string& aAnyHitShader = "",
	                                                                         const std::string& aIntersectionShader = "");

	void											destroy();

	std::vector<VkRayTracingShaderGroupCreateInfoKHR>& getShaderGroupCreateInfo();
	sbtInfo&										getRgenSBTInfo();
	sbtInfo&										getMissSBTInfo();
	sbtInfo&										getHitSBTInfo();
	sbtInfo&										getCallableSBTInfo();
private:
	bool											checkStage(Stage aStage) override;
													// check if shader groups are bundled in groups of RAYGEN - MISS - CLOSEST-HIT/ANY-HIT/INTERSECTION - CALLABLE
													// otherwise we can not create a correct shader binding table
	bool											checkShaderGroupOrderGeneral(Stage aNewStage);
	bool											checkShaderGroupOrderHit();

	std::vector<VkRayTracingShaderGroupCreateInfoKHR> mShaderGroupCreateInfos;

	int												mCurrentBaseAlignedOffset;

	sbtInfo											mRgen;
	sbtInfo											mMiss;
	sbtInfo											mHit;
	sbtInfo											mCallable;
};
RVK_END_NAMESPACE
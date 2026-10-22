#pragma once
#include <rvk/parts/rvk_public.hpp>

RVK_BEGIN_NAMESPACE
class LogicalDevice;
class Pipeline;
// base shader class for rasterizer, compute and ray tracing shader
class Shader {
public:
	enum class Source {
		GLSL,		// glslang compiler
		HLSL,		// glslang compiler
		HLSL_DXC,	// directX compiler
		SPV
	};
	enum Stage {
		ALL = VK_SHADER_STAGE_ALL,
		// rasterizer
		VERTEX = VK_SHADER_STAGE_VERTEX_BIT,
		TESS_CONTROL = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		TESS_EVALUATION = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		GEOMETRY = VK_SHADER_STAGE_GEOMETRY_BIT,
		FRAGMENT = VK_SHADER_STAGE_FRAGMENT_BIT,
		// compute
		COMPUTE = VK_SHADER_STAGE_COMPUTE_BIT,
		// ray tracing
		RAYGEN = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
		ANY_HIT = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
		CLOSEST_HIT = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		MISS = VK_SHADER_STAGE_MISS_BIT_KHR,
		INTERSECTION = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
		CALLABLE = VK_SHADER_STAGE_CALLABLE_BIT_KHR,
		RAY_TRACING = RAYGEN | ANY_HIT | CLOSEST_HIT | MISS | INTERSECTION | CALLABLE
	};

													// reserve memory for the amount of stages to add
	void											reserve(int aCount);
	// add a stage to this shader from shader code on disk or string
													// note: defines are ignored for source == Source::SPV
	void											addStage(Source aSource, Stage aStage, const std::string& aFilePath,
														const std::vector<std::string>&aDefines = {}, const std::string& aEntryPoint = "main");
	void											addStageFromString(Source aSource, Stage aStage, const std::string& aCode, 
																		const std::vector<std::string>& aDefines = {},
																		const std::string& aEntryPoint = "main");
													// add a constant definition and data to one stage (use after adding corresponding stage)
	void											addConstant(uint32_t aIndex, uint32_t aId, uint32_t aSize, uint32_t aOffset = 0);
	void											addConstant(const std::string& aFile, uint32_t aId, uint32_t aSize, uint32_t aOffset = 0);
													// set the data buffer for the constants
													// the data pointer must not be null at the time the pipeline using this shader is build
	void											setConstantData(uint32_t aIndex, const void* aData, uint32_t aSize);
	void											setConstantData(const std::string& aFile, const void* aData, uint32_t aSize);
													// finish shader and build shader modules
	void											finish();
													// if the shader with the given path is present, reload it and update associated pipelines
	void											reloadShader(int aIndex);
	void											reloadShader(const std::vector<int>& aIndices);
	void											reloadShader(const char* aFile);
	void											reloadShader(const std::vector<std::string>& aFiles);

	std::vector<VkPipelineShaderStageCreateInfo>&	getShaderStageCreateInfo();

	void											destroy();

	void											linkPipeline(Pipeline* aPipeline);
	void											unlinkPipeline(const Pipeline* aPipeline);

	static std::string								cache_dir;
	static bool										cache;

protected:
													Shader(LogicalDevice* aDevice) : mDevice(aDevice), mStageData({}), mShaderStageCreateInfos({}) { {} }
													~Shader();

													// find the index of a shader based on its file path
	int												findShaderIndex(const std::string& aFile) const;
													// check if the correct stages are used for the corresponding shader
	virtual bool									checkStage(Stage stage) = 0;

	struct stage_s {
		Source										source;													// the shaders source e.g. glsl or spv binary format
		Stage										stage;													// the stage of the shader
		std::string									file_path;												// path to the source/bin file
		std::vector<std::string>					defines;												// the defines to set for the shader
		std::string									entry_point;											// name of main function
		std::vector<VkSpecializationMapEntry>		const_entry;											// defines fixed const variables of stage
		VkSpecializationInfo						const_info;
		std::string									spv;													// spv data
	};
													// reads data.stage and data.file_path and sets data.spv
	static bool										loadShaderFromFile(stage_s* aData);
	static bool										loadShaderFromString(std::string const& aCode, stage_s* aData);
													// reads data.stage and data.file_path and sets data.spv
													// populate createInfo with data
	static void										configureStage(const LogicalDevice* aDevice, VkPipelineShaderStageCreateInfo* aCreateInfo, const stage_s* aData);

	LogicalDevice*									mDevice;
													// data at index i of stage_data is connected to data at index i of shaderStageCreateInfos
	std::vector<stage_s>							mStageData;								// data for each shader stage
	std::vector<VkPipelineShaderStageCreateInfo>	mShaderStageCreateInfos;					// the VkPipelineShaderStageCreateInfo array used by the vulkan api

													// pipelines associated with this shader
	std::deque<Pipeline*>							mPipelines;
};
RVK_END_NAMESPACE
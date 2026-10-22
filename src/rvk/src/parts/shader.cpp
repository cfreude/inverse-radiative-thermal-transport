#include <rvk/parts/shader.hpp>
#include <rvk/parts/rvk_public.hpp>
#include <rvk/parts/pipeline.hpp>
#include <rvk/rvk.hpp>
#include "rvk_private.hpp"

#include <fstream>
#include <sstream>
#include "shader_compiler.hpp"
RVK_USE_NAMESPACE

bool rvk::Shader::cache = false;
std::string rvk::Shader::cache_dir;

namespace {
    VkShaderStageFlagBits stageToVkShaderStage(const rvk::Shader::Stage aStage)
    {
        switch (aStage) {
            // rasterizer
        case rvk::Shader::Stage::VERTEX: return VK_SHADER_STAGE_VERTEX_BIT;
        case rvk::Shader::Stage::TESS_CONTROL: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case rvk::Shader::Stage::TESS_EVALUATION: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case rvk::Shader::Stage::GEOMETRY: return VK_SHADER_STAGE_GEOMETRY_BIT;
        case rvk::Shader::Stage::FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
            // compute
        case rvk::Shader::Stage::COMPUTE: return VK_SHADER_STAGE_COMPUTE_BIT;
            // ray tracing
        case rvk::Shader::Stage::RAYGEN: return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        case rvk::Shader::Stage::ANY_HIT: return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        case rvk::Shader::Stage::CLOSEST_HIT: return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        case rvk::Shader::Stage::MISS: return VK_SHADER_STAGE_MISS_BIT_KHR;
        case rvk::Shader::Stage::INTERSECTION: return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        case rvk::Shader::Stage::CALLABLE: return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        }
    }
    bool readFile(const std::string& aFilename, std::string& aFileContent) {
        std::ifstream file(aFilename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        const std::streamsize fileSize = file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        aFileContent = std::string(buffer.begin(), buffer.end());
        return true;
    }
    void createShaderModuleVulkan(const LogicalDevice* aDevice, VkShaderModule* aHandle, const std::string& aCode)
    {
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = aCode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(aCode.data());
        VK_CHECK(aDevice->vk.CreateShaderModule(aDevice->getHandle(), &createInfo, NULL, aHandle), "failed to create Shader Module!");
    }
}

void Shader::reserve(const int aCount)
{ mStageData.reserve(aCount); mShaderStageCreateInfos.reserve(aCount); }

void Shader::addStage(const Source aSource, const Stage aStage, const std::string& aFilePath,
                      const std::vector<std::string>& aDefines, const std::string& aEntryPoint){
    stage_s data = {};
    data.source = aSource;
    data.stage = aStage;
    data.file_path = aFilePath;
    data.defines = aDefines;
    data.entry_point = aEntryPoint;
    ASSERT(loadShaderFromFile(&data) == true, "Shader compilation error");
    mStageData.push_back(data);
}

void Shader::addStageFromString(const Source aSource, const Stage aStage, const std::string& aCode, 
    const std::vector<std::string>& aDefines, const std::string& aEntryPoint)
{
    stage_s data = {};
    data.source = aSource;
    data.stage = aStage;
    data.file_path = "";
    data.defines = aDefines;
    data.entry_point = aEntryPoint;
    ASSERT(loadShaderFromString(aCode, &data) == true, "Shader compilation error");
    mStageData.push_back(data);
}

void Shader::addConstant(const uint32_t aIndex, const uint32_t aId, const uint32_t aSize, const uint32_t aOffset)
{
    if (aIndex >= mStageData.size()) {
        Logger::error("rvk Shader: can not add constant to stage that is not present");
        return;
    }
    mStageData[aIndex].const_entry.push_back({ aId, aOffset, aSize });
    mStageData[aIndex].const_info.pMapEntries = mStageData[aIndex].const_entry.data();
    mStageData[aIndex].const_info.mapEntryCount = static_cast<uint32_t>(mStageData[aIndex].const_entry.size());
}

void Shader::addConstant(const std::string& aFile, const uint32_t aId, const uint32_t aSize, const uint32_t aOffset)
{
	const int index = findShaderIndex(aFile);
    if (index >= mStageData.size() || index == -1) {
        Logger::error("rvk Shader: can not add constant to stage that is not present");
        return;
    }
    addConstant(index, aId, aSize, aOffset);
}

void Shader::setConstantData(const uint32_t aIndex, const void* aData, const uint32_t aSize)
{
    if (aIndex >= mStageData.size() || aIndex == -1) {
        Logger::error("rvk Shader: can not set constant data to stage that is not present");
        return;
    }
    mStageData[aIndex].const_info.pMapEntries = mStageData[aIndex].const_entry.data();
    mStageData[aIndex].const_info.mapEntryCount = static_cast<uint32_t>(mStageData[aIndex].const_entry.size());
    mStageData[aIndex].const_info.pData = aData;
    mStageData[aIndex].const_info.dataSize = aSize;
}

void Shader::setConstantData(const std::string& aFile, const void* aData, const uint32_t aSize)
{
	const int index = findShaderIndex(aFile);
    if (static_cast<uint32_t>(index) >= mStageData.size()) {
        Logger::error("rvk Shader: can not set constant data to stage that is not present");
        return;
    }
    setConstantData(index, aData, aSize);
}

void Shader::finish()
{
    mShaderStageCreateInfos.resize(mStageData.size());
    int i = 0;
    for (stage_s& s : mStageData) {
        configureStage(mDevice, &mShaderStageCreateInfos[i], &s);
        i++;
    }
}

void Shader::reloadShader(const int aIndex)
{
    const std::vector v{ aIndex };
    reloadShader(v);
}

void Shader::reloadShader(const std::vector<int>& aIndices)
{
    for (const int index : aIndices) {
        if (index == -1) continue;
        if (mStageData[index].file_path.empty()) {
            Logger::info("It is not possible to reload a shader loaded from string");
            return;
        }
        Logger::info("Reloading Shader: " + mStageData[index].file_path);
        // load new shader
        loadShaderFromFile(&mStageData[index]);
        // destroy old shader module
        mDevice->vk.DestroyShaderModule(mDevice->getHandle(), mShaderStageCreateInfos[index].module, nullptr);
        // build new shader module
        configureStage(mDevice, &mShaderStageCreateInfos[index], &mStageData[index]);
    }
    // notify pipelines
    for (Pipeline* p : mPipelines) p->rebuildPipeline();
}

void Shader::reloadShader(const char* aFile)
{
    const std::vector<std::string> v{ aFile };
    reloadShader(v);
}

void Shader::reloadShader(const std::vector<std::string>& aFiles) {
    std::vector<int> indices;
    indices.reserve(aFiles.size());
    for (const std::string& file : aFiles) indices.push_back(findShaderIndex(file));
    reloadShader(indices);
}

std::vector<VkPipelineShaderStageCreateInfo>& Shader::getShaderStageCreateInfo()
{ return mShaderStageCreateInfos; }

void Shader::destroy()
{
    for (stage_s& s : mStageData) {
        s.defines.clear();
        s.file_path.clear();
        s.spv.clear();
    }

    mStageData.clear();
    mStageData = {};
    for (const VkPipelineShaderStageCreateInfo& i : mShaderStageCreateInfos) mDevice->vk.DestroyShaderModule(mDevice->getHandle(), i.module, VK_NULL_HANDLE);
    mShaderStageCreateInfos.clear();
    mShaderStageCreateInfos = {};
    mPipelines.clear();
    mPipelines = {};
}

void Shader::linkPipeline(Pipeline* aPipeline)
{
    if (std::find(mPipelines.begin(), mPipelines.end(), aPipeline) == mPipelines.end())
    {
        mPipelines.push_back(aPipeline);
    }
}

void Shader::unlinkPipeline(const Pipeline* aPipeline)
{
	const auto it = std::find(mPipelines.begin(), mPipelines.end(), aPipeline);
    if (it != mPipelines.end())
    {
        mPipelines.erase(it);
    }
}

// PROTECTED
rvk::Shader::~Shader()
{
    destroy();
}
int rvk::Shader::findShaderIndex(const std::string& aFile) const
{
    if (aFile.empty()) return -1;
    for (int i = 0; i < static_cast<int>(mStageData.size()); i++) {
        if (aFile == mStageData[i].file_path) return i;
    }
    return -1;
}
// STATIC
bool Shader::loadShaderFromFile(stage_s* const aData) {
    std::string code;
    if (!readFile(aData->file_path, code)) {
        Logger::error("Could not load: " + aData->file_path);
        return false;
    }
    return loadShaderFromString(code, aData);
}
bool Shader::loadShaderFromString(std::string const& aCode, stage_s* const aData)
{
    // load glsl or hlsl, look for cache, if not found compile to spv
    if (aData->source == rvk::Shader::Source::GLSL || aData->source == rvk::Shader::Source::HLSL || aData->source == rvk::Shader::Source::HLSL_DXC) {
        if (aData->source == rvk::Shader::Source::HLSL_DXC) scomp::preprocessDXC(aData->source, aData->stage, aData->file_path, aCode, aData->defines, aData->entry_point);
        else scomp::preprocess(aData->source, aData->stage, aData->file_path, aCode, aData->defines, aData->entry_point);
        std::ostringstream oss;
        oss << rvk::Shader::cache_dir << "/" << scomp::getHash() << ".spv";
        // looking for cache
        if (rvk::Shader::cache && readFile(oss.str(), aData->spv)) {
            scomp::cleanup();
            return true;
        }
        else {
            Logger::info("- Cache not found, compiling: " + aData->file_path);
            bool success = false;
            if (aData->source == rvk::Shader::Source::HLSL_DXC) success = scomp::compileDXC(aData->spv, rvk::Shader::cache, rvk::Shader::cache_dir);
            else success = scomp::compile(aData->spv, rvk::Shader::cache, rvk::Shader::cache_dir);
            scomp::cleanup();
            return success;
        }
    }
    // load spv
    else if (aData->source == rvk::Shader::Source::SPV) aData->spv = aCode;
    return true;
}
void Shader::configureStage(const LogicalDevice* aDevice, VkPipelineShaderStageCreateInfo* const aCreateInfo, const stage_s* const aData)
{
    aCreateInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    aCreateInfo->stage = stageToVkShaderStage(aData->stage);
    createShaderModuleVulkan(aDevice, &aCreateInfo->module, aData->spv);
    aCreateInfo->pName = aData->entry_point.c_str();

    // we could also set the shader consts here but then they need to be set before finishing the shader and the
    // shader can also not be added to other pipelines with different consts
    //aData->const_info.pMapEntries = aData->const_entry.data();
    //aData->const_info.mapEntryCount = static_cast<uint32_t>(aData->const_entry.size());
    aCreateInfo->pSpecializationInfo = &aData->const_info;
}
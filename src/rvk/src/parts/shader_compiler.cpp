#include "shader_compiler.hpp"
#include <rvk/shader_compiler.hpp>
#include <rvk/parts/logger.hpp>
#include <algorithm>
#include <sstream>
#include <fstream>
// wstring
#include <codecvt>
#include <locale>

// glslang
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <SPIRV/GlslangToSpv.h>
#include <SPIRV/disassemble.h>
// dxc
#if defined( __linux__) || defined(__APPLE__)
#define __EMULATE_UUID 1
#endif
#include <dxc/dxcapi.h>

namespace {
    std::wstring stageToDXCTarget(const rvk::Shader::Stage aStage)
    {
        switch (aStage) {
            // rasterizer
        case rvk::Shader::Stage::VERTEX: return L"vs_6_6";
        case rvk::Shader::Stage::TESS_CONTROL: return L"hs_6_6";        // Hull Shader
        case rvk::Shader::Stage::TESS_EVALUATION: return L"ds_6_6";     // Domain Shader
        case rvk::Shader::Stage::GEOMETRY: return L"gs_6_6";
        case rvk::Shader::Stage::FRAGMENT: return L"ps_6_6";            // Pixel Shader
            // compute
        case rvk::Shader::Stage::COMPUTE: return L"cs_6_6";
            // ray tracing
        case rvk::Shader::Stage::RAYGEN:                                  // Library
        case rvk::Shader::Stage::ANY_HIT:
        case rvk::Shader::Stage::CLOSEST_HIT:
        case rvk::Shader::Stage::MISS:
        case rvk::Shader::Stage::INTERSECTION:
        case rvk::Shader::Stage::CALLABLE:
        case rvk::Shader::Stage::RAY_TRACING: return L"lib_6_6";
        //  Amplification Shader 	as_6_6
        //  Mesh Shader 	        ms_6_6
        }
    }

    EShLanguage stageToEShLanguage(const rvk::Shader::Stage aStage)
    {
        switch (aStage) {
            // rasterizer
        case rvk::Shader::Stage::VERTEX: return EShLangVertex;
        case rvk::Shader::Stage::TESS_CONTROL: return EShLangTessControl;
        case rvk::Shader::Stage::TESS_EVALUATION: return EShLangTessEvaluation;
        case rvk::Shader::Stage::GEOMETRY: return EShLangGeometry;
        case rvk::Shader::Stage::FRAGMENT: return EShLangFragment;
            // compute
        case rvk::Shader::Stage::COMPUTE: return EShLangCompute;
            // ray tracing
        case rvk::Shader::Stage::RAYGEN: return EShLangRayGen;
        case rvk::Shader::Stage::ANY_HIT: return EShLangAnyHit;
        case rvk::Shader::Stage::CLOSEST_HIT: return EShLangClosestHit;
        case rvk::Shader::Stage::MISS: return EShLangMiss;
        case rvk::Shader::Stage::INTERSECTION: return EShLangIntersect;
        case rvk::Shader::Stage::CALLABLE: return EShLangCallable;
        }
    }

    glslang::EShSource sourceToEShSourceGlsl(const rvk::Shader::Source aSource)
    {
        switch (aSource) {
            case rvk::Shader::Source::GLSL: return glslang::EShSourceGlsl;
            case rvk::Shader::Source::HLSL: return glslang::EShSourceHlsl;
            default: return glslang::EShSourceNone;
        }
    }

    std::string getFilePath(const std::string& aStr)
    {
	    const size_t found = aStr.find_last_of("/\\");
        return aStr.substr(0, found);
        //size_t FileName = str.substr(found+1);
    }

    // from glslang StandAlone/DirStackFileIncluder.h
    // Default include class for normal include convention of search backward
    // through the stack of active include paths (for nested includes).
    // Can be overridden to customize.
    class DirStackFileIncluder : public glslang::TShader::Includer {
    public:
        DirStackFileIncluder() : externalLocalDirectoryCount(0) { }

        virtual IncludeResult* includeLocal(const char* headerName,
            const char* includerName,
            size_t inclusionDepth) override
        {
            return readLocalPath(headerName, includerName, static_cast<int>(inclusionDepth));
        }

        virtual IncludeResult* includeSystem(const char* headerName,
            const char* /*includerName*/,
            size_t /*inclusionDepth*/) override
        {
            return readSystemPath(headerName);
        }

        // Externally set directories. E.g., from a command-line -I<dir>.
        //  - Most-recently pushed are checked first.
        //  - All these are checked after the parse-time stack of local directories
        //    is checked.
        //  - This only applies to the "local" form of #include.
        //  - Makes its own copy of the path.
        virtual void pushExternalLocalDirectory(const std::string& dir)
        {
            directoryStack.push_back(dir);
            externalLocalDirectoryCount = (int)directoryStack.size();
        }

        virtual void releaseInclude(IncludeResult* result) override
        {
            if (result != nullptr) {
                delete[] static_cast<tUserDataElement*>(result->userData);
                delete result;
            }
        }

        virtual ~DirStackFileIncluder() override { }

    protected:
        typedef char tUserDataElement;
        std::vector<std::string> directoryStack;
        int externalLocalDirectoryCount;

        // Search for a valid "local" path based on combining the stack of include
        // directories and the nominal name of the header.
        virtual IncludeResult* readLocalPath(const char* headerName, const char* includerName, int depth)
        {
            // Discard popped include directories, and
            // initialize when at parse-time first level.
            directoryStack.resize(depth + externalLocalDirectoryCount);
            if (depth == 1)
                directoryStack.back() = getDirectory(includerName);

            // Find a directory that works, using a reverse search of the include stack.
            for (auto it = directoryStack.rbegin(); it != directoryStack.rend(); ++it) {
                std::string path = *it + '/' + headerName;
                std::replace(path.begin(), path.end(), '\\', '/');
                std::ifstream file(path, std::ios_base::binary | std::ios_base::ate);
                if (file) {
                    directoryStack.push_back(getDirectory(path));
                    return newIncludeResult(path, file, (int)file.tellg());
                }
            }

            return nullptr;
        }

        // Search for a valid <system> path.
        // Not implemented yet; returning nullptr signals failure to find.
        virtual IncludeResult* readSystemPath(const char* /*headerName*/) const
        {
            return nullptr;
        }

        // Do actual reading of the file, filling in a new include result.
        virtual IncludeResult* newIncludeResult(const std::string& path, std::ifstream& file, int length) const
        {
            char* content = new tUserDataElement[length];
            file.seekg(0, file.beg);
            file.read(content, length);
            return new IncludeResult(path, content, length, content);
        }

        // If no path markers, return current working directory.
        // Otherwise, strip file name and return path leading up to it.
        virtual std::string getDirectory(const std::string path) const
        {
	        const size_t last = path.find_last_of("/\\");
            return last == std::string::npos ? "." : path.substr(0, last);
        }
    };
}

namespace {
    // dxc
    IDxcUtils* utils;
    IDxcLibrary* library;
    IDxcCompiler* compiler;
    IDxcIncludeHandler* dxc_includer;
    std::wstring w_path;
    std::wstring w_entry_point;
    std::wstring w_target;
    std::vector<std::wstring> w_defines;
    std::vector<const wchar_t*> w_arguments;
    std::wstring w_preprocessedCode;
    IDxcBlob* blob_preprocessedCode;

    // glsl
    int ClientInputSemanticsVersion = 100;
    glslang::EShTargetClientVersion ClientVersion;
    glslang::EShClient Client = glslang::EShClientVulkan;
    glslang::EShTargetLanguage TargetLanguage = glslang::EShTargetSpv;
    glslang::EShTargetLanguageVersion TargetVersion;

    EShMessages messages = (EShMessages) (EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules | EShMsgReadHlsl);
    TBuiltInResource Resources = *GetDefaultResources();
    const int defaultVersion = 100;

    // for setting defines in shader
    std::string preamble;                   // like "#define GLSL\n#define OTHER\n"
    std::vector<std::string> processes;     // like { "define-macro GLSL", "define-macro OTHER" }

    glslang::TShader* shader = nullptr;
    EShLanguage stage;
    DirStackFileIncluder includer;
    std::size_t hash;
    std::string preprocessedCode;
}


void scomp::cleanup() {
    if (shader != nullptr) {
        delete shader;
        shader = nullptr;
    }
    includer = DirStackFileIncluder();
    hash = 0;
    preprocessedCode = "";
    preamble = "";
    processes.clear();

    ClientVersion = glslang::EShTargetVulkan_1_2;
    TargetVersion = glslang::EShTargetSpv_1_5;

    // dxc
    w_arguments.clear();
    w_defines.clear();
    if (dxc_includer != nullptr) {
        dxc_includer->Release();
        dxc_includer = nullptr;
    }
    if (utils) utils->CreateDefaultIncludeHandler(&dxc_includer);
    if (shader != nullptr) {
        blob_preprocessedCode->Release();
        blob_preprocessedCode = nullptr;
    }
}


void scomp::getGlslangVersion(int& aMajor, int& aMinor, int& aPatch)
{
	const glslang::Version glslangVersion = glslang::GetVersion();
    aMajor = glslangVersion.major;
	aMinor = glslangVersion.minor;
	aPatch = glslangVersion.patch;
}

void scomp::getDxcVersion(int& aMajor, int& aMinor, int& aPatch)
{
	IDxcVersionInfo* dxcVersion;
    uint32_t umajor, uminor;
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcVersion));
    dxcVersion->GetVersion(&umajor, &uminor);
	dxcVersion->Release();
    aMajor = static_cast<int>(umajor);
	aMinor = static_cast<int>(uminor);
    aPatch = 0;
}

void scomp::init()
{
    glslang::InitializeProcess();

    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
    DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
}

void scomp::finalize()
{
    cleanup();
    glslang::FinalizeProcess();

    //if (utils) utils->Release();
    //if (utils) library->Release();
    //if (utils) compiler->Release();
}

bool scomp::preprocessDXC(const rvk::Shader::Source aRvkSource, const rvk::Shader::Stage aRvkStage, std::string const& aPath, std::string const& aCode, std::vector<std::string> const& aDefines, std::string const& aEntryPoint) {
    cleanup();
    
    HRESULT hr;
    IDxcBlobEncoding* pSource;
    utils->CreateBlob(aCode.c_str(), aCode.size(), CP_UTF8, &pSource);

    // -E for the entry point, -T for the target profile
    w_entry_point = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(aEntryPoint);
    w_target = stageToDXCTarget(aRvkStage);
    w_defines.reserve(aDefines.size());
    w_arguments = { L"-HV", L"2021", L"-spirv", L"-E", w_entry_point.c_str(), L"-T", w_target.c_str(), L"-fspv-target-env=vulkan1.2"};
    for (const std::string& define : aDefines)
    {
        w_arguments.push_back(L"-D");
        w_defines.push_back(std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(define));
        w_arguments.push_back(w_defines.back().c_str());
    }

    w_path = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(aPath);
    IDxcOperationResult *result;
    hr = compiler->Preprocess(pSource, 
        w_path.c_str(),
        w_arguments.data(), w_arguments.size(), // pArguments, argCount
        NULL, 0, // pDefines, defineCount
        dxc_includer,
        &result);
    pSource->Release();

    if (SUCCEEDED(hr)) result->GetStatus(&hr);
    if (FAILED(hr) && result) {
        IDxcBlobEncoding *errorsBlob;
        hr = result->GetErrorBuffer(&errorsBlob);
        if (SUCCEEDED(hr) && errorsBlob) rvk::Logger::error("\n" + std::string(static_cast<const char*>(errorsBlob->GetBufferPointer())));
        errorsBlob->Release();
        return false;
    }

    result->GetResult(&blob_preprocessedCode);
    preprocessedCode.assign(static_cast<const char*>(blob_preprocessedCode->GetBufferPointer()), static_cast<int>(blob_preprocessedCode->GetBufferSize()));
    result->Release();

    hash = std::hash<std::string>{}(preprocessedCode);
    
    return true;
}

bool scomp::compileDXC(std::string& aSpv, const bool aCache, std::string const& aCacheDir) {
    HRESULT hr;
    IDxcOperationResult* result;
    hr = compiler->Compile(blob_preprocessedCode,
        w_path.c_str(),
        w_entry_point.c_str(),
        w_target.c_str(),
        w_arguments.data(), w_arguments.size(),
        nullptr, 0, // pDefines, defineCount
        dxc_includer,
        &result);
    blob_preprocessedCode->Release();

    if (SUCCEEDED(hr)) result->GetStatus(&hr);
    if (FAILED(hr) && result) {
        IDxcBlobEncoding* errorsBlob;
        if (SUCCEEDED(result->GetErrorBuffer(&errorsBlob)) && errorsBlob) rvk::Logger::error("\n" + std::string(static_cast<const char*>(errorsBlob->GetBufferPointer())));
        result->Release();
        errorsBlob->Release();
        return false;
    }

    IDxcBlob* spv_out;
    result->GetResult(&spv_out);
    result->Release();
    aSpv.assign(static_cast<const char*>(spv_out->GetBufferPointer()), static_cast<int>(spv_out->GetBufferSize()));
    spv_out->Release();

    // write spv to file
    if (aCache) {
        std::ostringstream oss;
        oss << aCacheDir << "/" << hash << ".spv";
        std::vector<uint8_t> vec(aSpv.begin(), aSpv.end());
        glslang::OutputSpvBin((std::vector<unsigned int>&)vec, oss.str().c_str());
    }

    return true;
}

bool scomp::preprocess(const rvk::Shader::Source aRvkSource, const rvk::Shader::Stage aRvkStage, std::string const& aPath, std::string const& aCode, std::vector<std::string> const& aDefines, std::string const& aEntryPoint) { 
    cleanup();
    const glslang::EShSource lang = sourceToEShSourceGlsl(aRvkSource);
    if (lang == glslang::EShSourceHlsl) {
        ClientVersion = glslang::EShTargetVulkan_1_1;
        TargetVersion = glslang::EShTargetSpv_1_3;
    }

    stage = stageToEShLanguage(aRvkStage);
    shader = new glslang::TShader(stage);
    const char* ccontent;
    ccontent = aCode.c_str();
    shader->setStrings(&ccontent, 1);
    shader->setEnvInput(lang, stage, Client, ClientInputSemanticsVersion);
    shader->setEnvClient(Client, ClientVersion);
    shader->setEnvTarget(TargetLanguage, TargetVersion);
    // entry point
    shader->setEntryPoint(aEntryPoint.c_str());
    // defines
    processes.reserve(aDefines.size());
    for (const std::string& s : aDefines) {
        if (s.empty()) continue;
        preamble.append("#define " + s + "\n");
        processes.push_back("define-macro " + s);
    }
    shader->setPreamble(preamble.c_str());
    shader->addProcesses(processes);

    // config includer
    includer.pushExternalLocalDirectory(getFilePath(aPath));

    // preprocess merges all includes into one source
    if (!shader->preprocess(&Resources, defaultVersion, ENoProfile, false, false, messages, &preprocessedCode, includer))
    {
        if (strlen(shader->getInfoLog()) > 0) rvk::Logger::error("\n" + std::string(shader->getInfoLog()));
        if (strlen(shader->getInfoDebugLog()) > 0) rvk::Logger::error("\n" + std::string(shader->getInfoDebugLog()));
        return false;
    }

    hash = std::hash<std::string>{}(preprocessedCode);
    return true;
}

std::size_t scomp::getHash()
{
    return hash;
}

bool scomp::compile(std::string& aSpv, const bool aCache, std::string const& aCacheDir) {
    const char* cpreprocessedCode = preprocessedCode.c_str();
    shader->setStrings(&cpreprocessedCode, 1);

    if (!shader->parse(&Resources, defaultVersion, false, messages, includer)) {
        if (strlen(shader->getInfoLog()) > 0) rvk::Logger::error("\n" + std::string(shader->getInfoLog()));
        if (strlen(shader->getInfoDebugLog()) > 0) rvk::Logger::error("\n" + std::string(shader->getInfoDebugLog()));
        return false;
    }
    glslang::TProgram program;
    program.addShader(shader);
    if (!program.link(messages)) {
        if (strlen(shader->getInfoLog()) > 0) rvk::Logger::error("\n" + std::string(shader->getInfoLog()));
        if (strlen(shader->getInfoDebugLog()) > 0) rvk::Logger::error("\n" + std::string(shader->getInfoDebugLog()));
        return false;
    }
    if (!program.mapIO()) {
        if (strlen(program.getInfoLog()) > 0) rvk::Logger::error("\n" + std::string(program.getInfoLog()));
        if (strlen(program.getInfoDebugLog()) > 0) rvk::Logger::error("\n" + std::string(program.getInfoDebugLog()));
        return false;
    }

    std::vector<unsigned int> spirv;
    if (program.getIntermediate(stage)) {

        std::string warningsErrors;
        spv::SpvBuildLogger logger;
        glslang::SpvOptions spvOptions;
        spvOptions.disableOptimizer = false;
        spvOptions.optimizeSize = false;
        spvOptions.disassemble = false;
        spvOptions.validate = false;
        glslang::GlslangToSpv(*program.getIntermediate(stage), spirv, &logger, &spvOptions);

        // print human readable spv code to cout
        //spv::Disassemble(std::cout, spirv);

        // write spv to file
        if (aCache) {
            std::ostringstream oss;
            oss << aCacheDir << "/" << hash << ".spv";
            glslang::OutputSpvBin(spirv, oss.str().c_str());
        }
    }

    std::string result;
    for (unsigned int i : spirv) {
        char c1 = (0x000000ff & i);
        char c2 = (0x0000ff00 & i) >> 8;
        char c3 = (0x00ff0000 & i) >> 16;
        char c4 = (0xff000000 & i) >> 24;
        result += c1;
        result += c2;
        result += c3;
        result += c4;
    };
    aSpv = result;
    return true;
}


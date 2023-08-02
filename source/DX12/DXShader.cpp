#include "pch.h"
#include "DXShader.h"
#include "DXUtil.h"
#include "Raekor/async.h"

namespace Raekor::DX12 {

ShaderCompiler g_ShaderCompiler;
SystemShadersDX12 g_SystemShaders;

RTTI_CLASS_CPP(ShaderProgram) {
    RTTI_MEMBER_CPP(ShaderProgram, SERIALIZE_JSON, "Defines",               mDefines);
    RTTI_MEMBER_CPP(ShaderProgram, SERIALIZE_JSON, "Vertex Shader File",    mVertexShaderFilePath);
    RTTI_MEMBER_CPP(ShaderProgram, SERIALIZE_JSON, "Pixel Shader File",     mPixelShaderFilePath);
    RTTI_MEMBER_CPP(ShaderProgram, SERIALIZE_JSON, "Compute Shader File",   mComputeShaderFilePath);

    //RTTI_MEMBER_CPP(ShaderProgram, SERIALIZE_BINARY, "Program Type",    mProgramType);
    //RTTI_MEMBER_CPP(ShaderProgram, SERIALIZE_BINARY, "Vertex Shader",   mVertexShader);
    //RTTI_MEMBER_CPP(ShaderProgram, SERIALIZE_BINARY, "Pixel Shader",    mPixelShader);
    //RTTI_MEMBER_CPP(ShaderProgram, SERIALIZE_BINARY, "Compute Shader",  mComputeShader);
}


RTTI_CLASS_CPP(SystemShadersDX12) {
    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "ImGui Shader",          mImGuiShader);
    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "Grass Shader",          mGrassShader);
    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Shader",        mGBufferShader);
    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "Lighting Shader",       mLightingShader);
    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "Downsample Shader",     mDownsampleShader);
    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "Debug Lines Shader",    mDebugLinesShader);
    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "Final Compose Shader",  mFinalComposeShader);

    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "Probe Debug Shader",             mProbeDebugShader);
    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "Probe Trace Shader",             mProbeTraceShader);
    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "Probe Update Depth Shader",      mProbeUpdateDepthShader);
    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "Probe Update Irradiance Shader", mProbeUpdateIrradianceShader);

    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "RT Shadows Shader",            mRTShadowsShader);
    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "RT Reflections Shader",        mRTReflectionsShader);
    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "RT Indirect Diffuse Shader",   mRTIndirectDiffuseShader);
    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "RT Ambient Occlusion Shader",  mRTAmbientOcclusionShader);


    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Debug Depth Shader",     mGBufferDebugDepthShader);
    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Debug Albedo Shader",    mGBufferDebugAlbedoShader);
    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Debug Normals Shader",   mGBufferDebugNormalsShader);
    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Debug Metallic Shader",  mGBufferDebugMetallicShader);
    RTTI_MEMBER_CPP(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Debug Roughness Shader", mGBufferDebugRoughnessShader);
}


bool ShaderProgram::GetGraphicsProgram(CD3DX12_SHADER_BYTECODE& ioVertexShaderByteCode, CD3DX12_SHADER_BYTECODE& ioPixelShaderByteCode) const {
    ioVertexShaderByteCode = CD3DX12_SHADER_BYTECODE(mVertexShader->GetBufferPointer(), mVertexShader->GetBufferSize());
    ioPixelShaderByteCode = CD3DX12_SHADER_BYTECODE(mPixelShader->GetBufferPointer(), mPixelShader->GetBufferSize());
    return mProgramType == SHADER_PROGRAM_GRAPHICS;
}


bool ShaderProgram::GetComputeProgram(CD3DX12_SHADER_BYTECODE& ioComputeShaderByteCode) const {
    ioComputeShaderByteCode = CD3DX12_SHADER_BYTECODE(mComputeShader->GetBufferPointer(), mComputeShader->GetBufferSize());
    return mProgramType == SHADER_PROGRAM_COMPUTE;
}


bool ShaderProgram::IsCompiled() const {
    switch (mProgramType) {
    case SHADER_PROGRAM_GRAPHICS:
        return mVertexShader != nullptr && mPixelShader != nullptr;
    case SHADER_PROGRAM_COMPUTE:
        return mComputeShader != nullptr;
    default:
        return false;
    }
}


ComPtr<IDxcBlob> ShaderCompiler::CompileShader(const Path& inPath, EShaderType inShaderType, const std::string& inDefines) {
    const auto shader_key = inPath.string() + inDefines;
    {
        auto lock = std::scoped_lock(m_ShaderCompilationMutex);
        if (m_ShaderCache.contains(shader_key))
            return m_ShaderCache.at(shader_key);
    }

    auto utils = ComPtr<IDxcUtils>{};
    auto library = ComPtr<IDxcLibrary>{};
    auto compiler = ComPtr<IDxcCompiler3>{};
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(utils.GetAddressOf()));
    DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(library.GetAddressOf()));
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(compiler.GetAddressOf()));

    auto include_handler = ComPtr<IDxcIncludeHandler>{};
    utils->CreateDefaultIncludeHandler(include_handler.GetAddressOf());

    auto ifs = std::ifstream(inPath);
    auto buffer = std::stringstream();
    buffer << inDefines << '\n';
    buffer << ifs.rdbuf();
    const auto source_str = buffer.str();

    auto blob = ComPtr<IDxcBlobEncoding>();
    gThrowIfFailed(library->CreateBlobWithEncodingFromPinned(source_str.c_str(), source_str.size(), CP_UTF8, blob.GetAddressOf()));

    auto arguments = std::vector<LPCWSTR>{};
    arguments.push_back(L"-E");
    arguments.push_back(L"main");

    arguments.push_back(L"-T");

    switch (inShaderType) {
    case SHADER_TYPE_VERTEX:  arguments.push_back(L"vs_6_6"); break;
    case SHADER_TYPE_PIXEL:   arguments.push_back(L"ps_6_6"); break;
    case SHADER_TYPE_COMPUTE: arguments.push_back(L"cs_6_6"); break;
    default: assert(false);
    }

    arguments.push_back(L"-Zi");
#ifndef NDEBUG
    arguments.push_back(L"-Qembed_debug");
    arguments.push_back(L"-Od");
#endif

    arguments.push_back(L"-I");
    arguments.push_back(L"assets/system/shaders/DirectX");

    arguments.push_back(L"-HV");
    arguments.push_back(L"2021");

    auto str_filepath = inPath.string();
    auto wstr_filepath = std::wstring(str_filepath.begin(), str_filepath.end());
    arguments.push_back(DXC_ARG_DEBUG_NAME_FOR_SOURCE);
    arguments.push_back(wstr_filepath.c_str());

    const auto source_buffer = DxcBuffer {
        .Ptr = blob->GetBufferPointer(),
        .Size = blob->GetBufferSize(),
        .Encoding = 0
    };

    auto result = ComPtr<IDxcResult>{};
    gThrowIfFailed(compiler->Compile(&source_buffer, arguments.data(), uint32_t(arguments.size()), include_handler.Get(), IID_PPV_ARGS(result.GetAddressOf())));

    auto hr_status = HRESULT{};
    result->GetStatus(&hr_status);

    auto errors = ComPtr<IDxcBlobUtf8>{};
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.GetAddressOf()), nullptr);

    if (errors && errors->GetStringLength() > 0) {
        auto lock = std::scoped_lock(m_ShaderCompilationMutex);
        auto error_c_str = static_cast<char*>(errors->GetBufferPointer());
        std::cout << error_c_str << '\n';

        auto error_str = std::string();
        auto line_nr = 0, char_nr = 0;

        auto token = strtok(error_c_str, ":");

        if (strncmp(token, "error", strlen("error")) != 0) {
            token = strtok(NULL, ":"); // File path/name is ignored
            line_nr = atoi(token);
            token = strtok(NULL, ":");
            char_nr = atoi(token);
            token = strtok(NULL, ":");
            error_str = std::string(token);
        }

        token = strtok(NULL, ":");
        const auto error_msg = std::string(token);

        OutputDebugStringA(std::format("{}({}): Error: {}", FileSystem::absolute(inPath).string(), line_nr, error_msg).c_str());

    }

    if (!SUCCEEDED(hr_status)) {
        std::cout << "Compilation " << COUT_RED("failed") << " for shader: " << inPath.string() << '\n';
        return nullptr;
    }

    auto shader = ComPtr<IDxcBlob>{}, pdb = ComPtr<IDxcBlob>{};
    auto debug_data = ComPtr<IDxcBlobUtf16>{};
    result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(shader.GetAddressOf()), debug_data.GetAddressOf());
    result->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(pdb.GetAddressOf()), debug_data.GetAddressOf());

    auto pdb_file = std::ofstream(inPath.parent_path() / inPath.filename().replace_extension(".pdb"));
    pdb_file.write((char*)pdb->GetBufferPointer(), pdb->GetBufferSize());

    auto lock = std::scoped_lock(m_ShaderCompilationMutex);
    m_ShaderCache.insert({ shader_key , shader });

    std::cout << "Compilation " << COUT_GREEN("Finished") << " for shader: " << inPath.string() << '\n';

    return shader;
}


bool ShaderCompiler::CompileShaderProgram(ShaderProgram& inShaderProgram) {
    if (!inShaderProgram.mVertexShaderFilePath.empty() && !inShaderProgram.mPixelShaderFilePath.empty() && inShaderProgram.mComputeShaderFilePath.empty()) {
        inShaderProgram.mProgramType = SHADER_PROGRAM_GRAPHICS;
    }
    else if (inShaderProgram.mVertexShaderFilePath.empty() && inShaderProgram.mPixelShaderFilePath.empty() && !inShaderProgram.mComputeShaderFilePath.empty()) {
        inShaderProgram.mProgramType = SHADER_PROGRAM_COMPUTE;
    }

    if (inShaderProgram.mProgramType == SHADER_PROGRAM_INVALID)
        return false;

    switch (inShaderProgram.mProgramType) {
    case SHADER_PROGRAM_GRAPHICS: {
        inShaderProgram.mVertexShader = CompileShader(inShaderProgram.mVertexShaderFilePath, SHADER_TYPE_VERTEX, inShaderProgram.mDefines);
        inShaderProgram.mPixelShader = CompileShader(inShaderProgram.mPixelShaderFilePath, SHADER_TYPE_PIXEL, inShaderProgram.mDefines);
        return inShaderProgram.mVertexShader != nullptr && inShaderProgram.mPixelShader != nullptr;
    }
    case SHADER_PROGRAM_COMPUTE: {
        inShaderProgram.mComputeShader = CompileShader(inShaderProgram.mComputeShaderFilePath, SHADER_TYPE_COMPUTE, inShaderProgram.mDefines);
        return inShaderProgram.mComputeShader != nullptr;
    }
    default:
        return false;
    }

    return false;
}


void SystemShadersDX12::CompileShaders() {
    for (const auto& member : GetRTTI()) {
        g_ThreadPool.QueueJob([this, &member]() {
            auto& shader_program = member->GetMemberRef<ShaderProgram>(this);
            g_ShaderCompiler.CompileShaderProgram(shader_program);
        });
    }
}

} // Raekor::DX12
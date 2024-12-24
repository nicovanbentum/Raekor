#include "pch.h"
#include "Shader.h"
#include "Hash.h"
#include "Timer.h"
#include "Member.h"
#include "Device.h"
#include "Threading.h"
#include "RenderUtil.h"
#include "RenderGraph.h"

namespace RK::DX12 {

ShaderCompiler g_ShaderCompiler;
SystemShadersDX12 g_SystemShaders;

RTTI_DEFINE_TYPE(IResource)
{
    // Base class interface only
}

RTTI_DEFINE_TYPE(Shader)
{
    RTTI_DEFINE_MEMBER(Shader, SERIALIZE_JSON, "Defines", mDefines);
    RTTI_DEFINE_MEMBER(Shader, SERIALIZE_JSON, "File Path", mFilePath);
    RTTI_DEFINE_MEMBER(Shader, SERIALIZE_BINARY, "Binary", mBinary);
}

RTTI_DEFINE_TYPE(GraphicsProgram)
{
    RTTI_DEFINE_TYPE_INHERITANCE(GraphicsProgram, IResource);

    RTTI_DEFINE_MEMBER(GraphicsProgram, SERIALIZE_JSON, "Defines", m_Defines);
    RTTI_DEFINE_MEMBER(GraphicsProgram, SERIALIZE_ALL, "Vertex Shader", m_VertexShader);
    RTTI_DEFINE_MEMBER(GraphicsProgram, SERIALIZE_ALL, "Pixel Shader", m_PixelShader);
    RTTI_DEFINE_MEMBER(GraphicsProgram, SERIALIZE_ALL, "Hull Shader", m_HullShader);
    RTTI_DEFINE_MEMBER(GraphicsProgram, SERIALIZE_ALL, "Domain Shader", m_DomainShader);
}

RTTI_DEFINE_TYPE(ComputeProgram)
{
    RTTI_DEFINE_TYPE_INHERITANCE(ComputeProgram, IResource);

    RTTI_DEFINE_MEMBER(ComputeProgram, SERIALIZE_JSON, "Defines", m_Defines);
    RTTI_DEFINE_MEMBER(ComputeProgram, SERIALIZE_ALL, "Compute Shader", m_ComputeShader);
}

RTTI_DEFINE_TYPE(SystemShadersDX12)
{
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Clear Buffer Shader", mClearBufferShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Clear Texture Shader", mClearTextureShader);

    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "ImGui Shader", mImGuiShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Grass Shader", mGrassShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Shader", mGBufferShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Skinning Shader", mSkinningShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Lighting Shader", mLightingShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Light Cull Shader", mLightCullShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Shadow Map Shader", mShadowMapShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Downsample Shader", mDownsampleShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "TAA Resolve Shader", mTAAResolveShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Final Compose Shader", mFinalComposeShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Debug Primitives Shader", mDebugPrimitivesShader);

    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "SSR Trace Shader", mSSRTraceShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "SSAO Trace Shader", mSSAOTraceShader);

    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Sky Cube Shader", mSkyCubeShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Convolve Cube Shader", mConvolveCubeShader);

    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Probe Debug Shader", mProbeDebugShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Probe Debug Rays Shader", mProbeDebugRaysShader);

    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Probe Trace Shader", mProbeTraceShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Probe Sample Shader", mProbeSampleShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Probe Update Depth Shader", mProbeUpdateDepthShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Probe Update Irradiance Shader", mProbeUpdateIrradianceShader);

    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "RT Path Trace Shader", mRTPathTraceShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "RT Reflections Shader", mRTReflectionsShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "RT Ambient Occlusion Shader", mRTAmbientOcclusionShader);

    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Trace Shadow Rays Shader", mTraceShadowRaysShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Clear Shadow Tiles Shader", mClearShadowTilesShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Classify Shadow Tiles Shader", mClassifyShadowTilesShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Denoise Shadow Tiles Shader", mDenoiseShadowTilesShader);

    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Debug Depth Shader", mGBufferDebugDepthShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Debug Albedo Shader", mGBufferDebugAlbedoShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Debug Normals Shader", mGBufferDebugNormalsShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Debug Emissive Shader", mGBufferDebugEmissiveShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Debug Velocity Shader", mGBufferDebugVelocityShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Debug Metallic Shader", mGBufferDebugMetallicShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Debug Roughness Shader", mGBufferDebugRoughnessShader);

    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Depth of Field Shader", mDepthOfFieldShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Bloom Upscale Shader", mBloomUpSampleShader);
    RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Bloom Downscale Shader", mBloomDownsampleShader);
}



bool Shader::OnCompile(Device& inDevice, EShaderType inType, const String& inProgramDefines)
{
    mFileTime = fs::last_write_time(mFilePath);

    ComPtr<IDxcBlob> blob = g_ShaderCompiler.CompileShader(mFilePath, inType, mDefines + inProgramDefines, mHash);

    if (blob == nullptr)
        return false;

    mBinary.resize(blob->GetBufferSize());
    std::memcpy(mBinary.data(), blob->GetBufferPointer(), blob->GetBufferSize());

    return IsCompiled();
}


bool ComputeProgram::CompilePSO(Device& inDevice, const char* inDebugName)
{
    Timer timer;
    m_ComputePipeline = nullptr;

    D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = { 
        .pRootSignature = inDevice.GetGlobalRootSignature(), 
        .CS = m_ComputeShader.GetShaderByteCode()
    };

    gThrowIfFailed(inDevice->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&m_ComputePipeline)));

    if (m_ComputePipeline && inDebugName)
    {
        std::cout << std::format("[DX12] Compute PSO {} compilation took {:.2f} ms \n", inDebugName, Timer::sToMilliseconds(timer.GetElapsedTime()));
        m_ComputePipeline->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(inDebugName), inDebugName);
    }

    return m_ComputePipeline != nullptr;
}


bool GraphicsProgram::OnCompile(Device& inDevice)
{
    auto CompileShader = [this](Shader& inShader, EShaderType inType) -> bool
    {
        inShader.mFileTime = fs::last_write_time(inShader.mFilePath);

        ComPtr<IDxcBlob> blob = g_ShaderCompiler.CompileShader(inShader.mFilePath, inType, m_Defines, inShader.mHash);

        if (blob == nullptr)
            return false;

        inShader.mBinary.resize(blob->GetBufferSize());
        std::memcpy(inShader.mBinary.data(), blob->GetBufferPointer(), blob->GetBufferSize());

        return inShader.IsCompiled();
    };

    const bool vs_compiled = m_VertexShader.OnCompile(inDevice, SHADER_TYPE_VERTEX, m_Defines);
    const bool ps_compiled = m_PixelShader.OnCompile(inDevice, SHADER_TYPE_PIXEL, m_Defines);

    if (!vs_compiled || !ps_compiled)
        return false;

    if (m_HullShader.HasFilePath() && m_DomainShader.HasFilePath())
    {
        const bool hs_compiled = m_HullShader.OnCompile(inDevice, SHADER_TYPE_HULL, m_Defines);
        const bool ds_compiled = m_DomainShader.OnCompile(inDevice, SHADER_TYPE_DOMAIN, m_Defines);

        if (!hs_compiled || !ds_compiled)
            return false;
    }

    return true;
}


bool Shader::IsOutOfDate() const 
{
    std::error_code error_code;
    fs::file_time_type timestamp = fs::last_write_time(mFilePath, error_code);
    while (error_code)
        timestamp = fs::last_write_time(mFilePath, error_code);

    return !mFilePath.empty() && mFileTime < timestamp;
}


bool GraphicsProgram::OnHotLoad(Device& inDevice)
{
    return false;
}


bool ComputeProgram::OnCompile(Device& inDevice)
{
    if (!m_ComputeShader.OnCompile(inDevice, SHADER_TYPE_COMPUTE, m_Defines))
        return false;

    return CompilePSO(inDevice);
}


bool ComputeProgram::OnHotLoad(Device& inDevice)
{
    uint64_t cs_hash = m_ComputeShader.GetHash();

    if (m_ComputeShader.IsOutOfDate())
        OnCompile(inDevice);

    bool new_hash = cs_hash != m_ComputeShader.GetHash();

    return new_hash && IsCompiled();
}


ComPtr<IDxcBlob> ShaderCompiler::CompileShader(const Path& inPath, EShaderType inShaderType, const String& inDefines, uint64_t& outHash)
{
    std::ifstream ifs(inPath);
    std::stringstream buffer;
    buffer << inDefines << '\n';
    buffer << ifs.rdbuf();
    
    return CompileShader(inPath, buffer.str(), inShaderType, inDefines, outHash);
}


ComPtr<IDxcBlob> ShaderCompiler::CompileShader(const Path& inPath, const String& inSource, EShaderType inShaderType, const String& inDefines, uint64_t& outHash)
{
    ComPtr<IDxcUtils> utils = nullptr;
    ComPtr<IDxcLibrary> library = nullptr;
    ComPtr<IDxcCompiler3> compiler = nullptr;
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(utils.GetAddressOf()));
    DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(library.GetAddressOf()));
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(compiler.GetAddressOf()));

    ComPtr<IDxcIncludeHandler> include_handler = nullptr;
    utils->CreateDefaultIncludeHandler(include_handler.GetAddressOf());

    ComPtr<IDxcBlobEncoding> blob = nullptr;
    gThrowIfFailed(library->CreateBlobWithEncodingFromPinned(inSource.c_str(), inSource.size(), CP_UTF8, blob.GetAddressOf()));

    Array<LPCWSTR> arguments;
    arguments.push_back(L"-E");
    arguments.push_back(L"main");

    arguments.push_back(L"-T");
    switch (inShaderType)
    {
        case SHADER_TYPE_VERTEX:  arguments.push_back(L"vs_6_6"); break;
        case SHADER_TYPE_HULL:    arguments.push_back(L"hs_6_6"); break;
        case SHADER_TYPE_DOMAIN:  arguments.push_back(L"ds_6_6"); break;
        case SHADER_TYPE_PIXEL:   arguments.push_back(L"ps_6_6"); break;
        case SHADER_TYPE_COMPUTE: arguments.push_back(L"cs_6_6"); break;
        default: assert(false);
    }

    arguments.push_back(L"-Zi");
#ifndef NDEBUG
    arguments.push_back(L"-Qembed_debug");
    arguments.push_back(L"-Od");
#endif

    arguments.push_back(L"-HV");
    arguments.push_back(L"2021");

    arguments.push_back(L"-I");
    arguments.push_back(L"Assets\\Shaders\\Backend\\");

    String str_filepath = inPath.string();
    WString wstr_filepath = WString(str_filepath.begin(), str_filepath.end());
    arguments.push_back(DXC_ARG_DEBUG_NAME_FOR_SOURCE);
    arguments.push_back(wstr_filepath.c_str());

    DxcBuffer source_buffer =
    {
        .Ptr = blob->GetBufferPointer(),
        .Size = blob->GetBufferSize()
    };

    arguments.push_back(L"-P");

    ComPtr<IDxcResult> pp_result = nullptr;
    gThrowIfFailed(compiler->Compile(&source_buffer, arguments.data(), uint32_t(arguments.size()), include_handler.Get(), IID_PPV_ARGS(pp_result.GetAddressOf())));

    HRESULT hr_status;
    gThrowIfFailed(pp_result->GetStatus(&hr_status));

    ComPtr<IDxcBlobUtf8> preprocessed_hlsl = nullptr;
    gThrowIfFailed(pp_result->GetOutput(DXC_OUT_HLSL, IID_PPV_ARGS(preprocessed_hlsl.GetAddressOf()), nullptr));

    outHash = gHashFNV1a((const char*)preprocessed_hlsl->GetBufferPointer(), preprocessed_hlsl->GetBufferSize());

    if (m_EnableShaderCache)
    {
        {
            std::scoped_lock lock = std::scoped_lock(m_ShaderCompilationMutex);
            if (m_ShaderCache.contains(outHash))
                return m_ShaderCache.at(outHash);
        }
    }

    arguments.pop_back(); // pop -P preprocessor flag

    source_buffer = DxcBuffer
    {
        .Ptr = preprocessed_hlsl->GetBufferPointer(),
        .Size = preprocessed_hlsl->GetBufferSize(),
        .Encoding = 0
    };

    ComPtr<IDxcResult> result = nullptr;
    gThrowIfFailed(compiler->Compile(&source_buffer, arguments.data(), uint32_t(arguments.size()), include_handler.Get(), IID_PPV_ARGS(result.GetAddressOf())));

    ComPtr<IDxcBlobUtf8> errors = nullptr;
    gThrowIfFailed(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.GetAddressOf()), nullptr));

    if (errors && errors->GetStringLength() > 0)
    {
        std::scoped_lock lock = std::scoped_lock(m_ShaderCompilationMutex);

        char* error_c_str = (char*)errors->GetBufferPointer();
        std::cout << error_c_str << '\n';

        String error_str;
        int line_nr = 0, char_nr = 0;

        char* token = strtok(error_c_str, ":");

        if (strncmp(token, "error", strlen("error")) != 0)
        {
            token = strtok(NULL, ":"); // File path/name is ignored
            line_nr = atoi(token);
            token = strtok(NULL, ":");
            char_nr = atoi(token);
            token = strtok(NULL, ":");
            error_str = String(token);
        }

        token = strtok(NULL, ":");
        const String error_msg = String(token);

        OutputDebugStringA(std::format("{}({}): Error: {}", fs::absolute(inPath).string(), line_nr, error_msg).c_str());
    }

    if (!SUCCEEDED(hr_status))
    {
        outHash = 0;
        std::cout << std::format("[DX12] Compilation {} for shader: {} \n", COUT_RED("failed"), inPath.string());
        return nullptr;
    }

    ComPtr<IDxcBlob> shader = nullptr, pdb = nullptr;
    ComPtr<IDxcBlobUtf16> debug_data = nullptr;
    hr_status = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(shader.GetAddressOf()), debug_data.GetAddressOf());
    hr_status = result->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(pdb.GetAddressOf()), debug_data.GetAddressOf());

    if (!SUCCEEDED(hr_status))
    {
        outHash = 0;
        std::cout << std::format("[DX12] Compilation {} for shader: {} \n", COUT_RED("failed"), inPath.string());
        return nullptr;
    }

    std::ofstream pdb_file = std::ofstream(inPath.parent_path() / inPath.filename().replace_extension(".pdb"));
    pdb_file.write((char*)pdb->GetBufferPointer(), pdb->GetBufferSize());

    if (m_EnableShaderCache)
    {
        std::scoped_lock lock = std::scoped_lock(m_ShaderCompilationMutex);
        m_ShaderCache.insert({ outHash, shader });
    }

    std::cout << std::format("[DX12] Compilation {} for shader: {} \n", COUT_GREEN("finished"), inPath.string());

    return shader;
}


void ShaderCompiler::ReleaseShader(uint64_t inHash)
{ 
    std::scoped_lock lock(m_ShaderCompilationMutex);  
    m_ShaderCache.erase(inHash); 
}


ID3D12PipelineState* ShaderCompiler::GetGraphicsPipeline(Device& inDevice, IRenderPass* inRenderPass, uint64_t inVertexShaderHash, uint64_t inPixelShaderHash)
{
    if (inVertexShaderHash == 0 || inPixelShaderHash == 0)
        return nullptr; 

    if (!m_ShaderCache.contains(inVertexShaderHash) || !m_ShaderCache.contains(inPixelShaderHash))
        return nullptr;

    const StaticArray hash_data = { inVertexShaderHash, inPixelShaderHash };
    const uint32_t shader_hash = gHashFNV1a((const char*)hash_data.data(), sizeof(hash_data[0]) * hash_data.size());

    std::scoped_lock lock(m_ShaderCompilationMutex);

    if (m_EnablePipelineCache && m_PipelineCache.contains(shader_hash))
    {
        return m_PipelineCache[shader_hash].Get();
    }
    else
    {
        GraphicsProgram program;
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = inRenderPass->CreatePipelineStateDesc(inDevice, program);

        desc.VS = CD3DX12_SHADER_BYTECODE(m_ShaderCache[inVertexShaderHash]->GetBufferPointer(), m_ShaderCache[inVertexShaderHash]->GetBufferSize());
        desc.PS = CD3DX12_SHADER_BYTECODE(m_ShaderCache[inPixelShaderHash]->GetBufferPointer(), m_ShaderCache[inPixelShaderHash]->GetBufferSize());

        inDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(m_PipelineCache[shader_hash].GetAddressOf()));

        return m_PipelineCache[shader_hash].Get();
    }

    assert(false);
    return nullptr;
}


bool SystemShadersDX12::OnHotLoad(Device& inDevice)
{
    // g_ShaderCompiler.DisableShaderCache();

    for (const auto& member : this->GetRTTI())
    {
        IResource* shader_program = member->Get<IResource>(this);

        if (shader_program->OnHotLoad(inDevice))
            return true;
    }

    // g_ShaderCompiler.EnableShaderCache();
    return false;
}



bool SystemShadersDX12::OnCompile(Device& inDevice)
{
    for (const auto& member : GetRTTI())
    {
        g_ThreadPool.QueueJob([&]()
        {
            IResource* shader_program = member->Get<IResource>(this);

            if (!shader_program->IsCompiled())
                shader_program->OnCompile(inDevice);
        });
    }

    return true;
}




bool SystemShadersDX12::IsCompiled() const
{
    bool is_compiled = true;

    for (const auto& member : GetRTTI())
    {
        const IResource* shader_program = member->Get<IResource>(this);
        is_compiled &= shader_program->IsCompiled();
    }

    return is_compiled;
}


} // Raekor::DX12
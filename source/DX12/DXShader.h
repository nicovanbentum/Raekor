#pragma once

namespace Raekor::DX12 {

class Device;
class ShaderCompiler;
class SystemShadersDX12;

extern ShaderCompiler g_ShaderCompiler;
extern SystemShadersDX12 g_SystemShaders;


enum EShaderProgramType
{
    SHADER_PROGRAM_INVALID,
    SHADER_PROGRAM_GRAPHICS,
    SHADER_PROGRAM_COMPUTE
};
RTTI_DECLARE_ENUM(EShaderProgramType);
RTTI_ENUM_STRING_CONVERSIONS(EShaderProgramType);


enum EShaderType
{
    SHADER_TYPE_VERTEX,
    SHADER_TYPE_PIXEL,
    SHADER_TYPE_COMPUTE
};

class IResource
{
    RTTI_DECLARE_TYPE(IResource);

public:
    virtual bool OnCompile() { return true; }
    virtual bool IsCompiled() const { return true; }
};


class ShaderProgram : public IResource
{
    RTTI_DECLARE_TYPE(ShaderProgram);
    friend class SystemShadersDX12;

public:
    bool GetGraphicsProgram(CD3DX12_SHADER_BYTECODE& ioVertexShaderByteCode, CD3DX12_SHADER_BYTECODE& ioPixelShaderByteCode) const;
    bool GetComputeProgram(CD3DX12_SHADER_BYTECODE& ioComputeShaderByteCode) const;

    inline bool IsCompute() const { return mProgramType == SHADER_PROGRAM_COMPUTE; }
    inline bool IsGraphics() const { return mProgramType == SHADER_PROGRAM_GRAPHICS; }

    EShaderProgramType GetProgramType() const { return mProgramType; }

    bool CompilePSO(Device& inDevice, const char* inDebugName = nullptr);

    ID3D12PipelineState* GetComputePSO() { return m_ComputePipeline.Get(); }

    bool OnCompile() override;
    bool IsCompiled() const override;

private:
    // JSON fields
    std::string mDefines = "";
    Path mVertexShaderFilePath;
    Path mPixelShaderFilePath;
    Path mComputeShaderFilePath;

    // Binary fields
    EShaderProgramType mProgramType;
    uint64_t mVertexShaderHash;
    uint64_t mPixelShaderHash;
    uint64_t mComputeShaderHash;
    std::vector<unsigned char> mVertexShader;
    std::vector<unsigned char> mPixelShader;
    std::vector<unsigned char> mComputeShader;

    // runtime fields
    fs::file_time_type mVertexShaderFileTime;
    fs::file_time_type mPixelShaderFileTime;
    fs::file_time_type mComputeShaderFileTime;
    ComPtr<ID3D12PipelineState> m_ComputePipeline = nullptr;
};


class ShaderCompiler
{
public:
    ComPtr<IDxcBlob> CompileShader(const Path& inPath, EShaderType inShaderType, const std::string& inDefines, uint64_t& outHash);

    void EnableShaderCache() { m_EnableCache.store(true); }
    void DisableShaderCache() { m_EnableCache.store(false); }

private:
    std::mutex m_ShaderCompilationMutex;
    std::atomic_bool m_EnableCache = true;
    std::unordered_map<uint64_t, ComPtr<IDxcBlob>> m_ShaderCache;
};


struct SystemShadersDX12 : public IResource
{
    SystemShadersDX12() = default;
    ~SystemShadersDX12() = default;

    NO_COPY_NO_MOVE(SystemShadersDX12);
    RTTI_DECLARE_TYPE(SystemShadersDX12);

    ShaderProgram mClearBufferShader;
    ShaderProgram mClearTextureShader;

    ShaderProgram mImGuiShader;
    ShaderProgram mGrassShader;
    ShaderProgram mGBufferShader;
    ShaderProgram mLightingShader;
    ShaderProgram mDownsampleShader;
    ShaderProgram mDebugLinesShader;
    ShaderProgram mTAAResolveShader;
    ShaderProgram mFinalComposeShader;

    ShaderProgram mProbeDebugShader;
    ShaderProgram mProbeTraceShader;
    ShaderProgram mProbeUpdateDepthShader;
    ShaderProgram mProbeUpdateIrradianceShader;

    ShaderProgram mRTShadowsShader;
    ShaderProgram mRTReflectionsShader;
    ShaderProgram mRTPathTraceShader;
    ShaderProgram mRTAmbientOcclusionShader;

    ShaderProgram mGBufferDebugDepthShader;
    ShaderProgram mGBufferDebugAlbedoShader;
    ShaderProgram mGBufferDebugNormalsShader;
    ShaderProgram mGBufferDebugMetallicShader;
    ShaderProgram mGBufferDebugRoughnessShader;

    ShaderProgram mDepthOfFieldShader;
    ShaderProgram mBloomUpSampleShader;
    ShaderProgram mBloomDownsampleShader;
    ShaderProgram mChromaticAberrationShader;

    bool HotLoad(Device& inDevice);
    bool CompilePSOs(Device& inDevice);

    bool OnCompile() override;
    bool IsCompiled() const override;
};



} // Raekor::DX12
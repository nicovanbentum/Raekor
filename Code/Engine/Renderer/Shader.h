#pragma once

#include "rtti.h"
#include "defines.h"

namespace RK::DX12 {

class Device;
class IRenderPass;
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
    RTTI_DECLARE_VIRTUAL_TYPE(IResource);

public:
    virtual bool OnCompile() { return true; }
    virtual bool IsCompiled() const { return true; }
};


class ShaderProgram : public IResource
{
    RTTI_DECLARE_VIRTUAL_TYPE(ShaderProgram);
    friend class SystemShadersDX12;

public:
    bool GetGraphicsProgram(ByteSlice& ioVertexShaderByteCode, ByteSlice& ioPixelShaderByteCode) const;
    bool GetComputeProgram(ByteSlice& ioComputeShaderByteCode) const;

    inline bool IsCompute() const { return mProgramType == SHADER_PROGRAM_COMPUTE; } 
    inline bool IsGraphics() const { return mProgramType == SHADER_PROGRAM_GRAPHICS; }

    uint64_t GetPixelShaderHash() const { return mPixelShaderHash; }
    uint64_t GetVertexShaderHash() const { return mVertexShaderHash; }
    uint64_t GetComputeShaderHash() const { return mComputeShaderHash; }

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
    Array<unsigned char> mVertexShader;
    Array<unsigned char> mPixelShader;
    Array<unsigned char> mComputeShader;

    // runtime fields
    fs::file_time_type mVertexShaderFileTime;
    fs::file_time_type mPixelShaderFileTime;
    fs::file_time_type mComputeShaderFileTime;

    ComPtr<ID3D12PipelineState> m_ComputePipeline = nullptr;
};


class ShaderCompiler
{
public:
    ComPtr<IDxcBlob> CompileShader(const Path& inPath, EShaderType inShaderType, const String& inDefines, uint64_t& outHash);
    ComPtr<IDxcBlob> CompileShader(const Path& inPath, const String& inSource, EShaderType inShaderType, const String& inDefines, uint64_t& outHash);
    void ReleaseShader(uint64_t inHash);

    ID3D12PipelineState* GetGraphicsPipeline(Device& inDevice, IRenderPass* inRenderPass, uint64_t inVertexShaderHash, uint64_t inPixelShaderHash);

    void SetShaderCacheEnabled(bool inEnabled) { m_EnableShaderCache = inEnabled; }
    void SetPipelineCacheEnabled(bool inEnabled) { m_EnablePipelineCache = inEnabled; }

private:
    std::mutex m_ShaderCompilationMutex;
    Atomic<bool> m_EnableShaderCache = true;
    Atomic<bool> m_EnablePipelineCache = true;
    HashMap<uint64_t, ComPtr<IDxcBlob>> m_ShaderCache;
    HashMap<uint64_t, ComPtr<ID3D12PipelineState>> m_PipelineCache;
};


struct SystemShadersDX12 : public IResource
{
    SystemShadersDX12() = default;
    ~SystemShadersDX12() = default;

    NO_COPY_NO_MOVE(SystemShadersDX12);
    RTTI_DECLARE_VIRTUAL_TYPE(SystemShadersDX12);

    ShaderProgram mClearBufferShader;
    ShaderProgram mClearTextureShader;

    ShaderProgram mImGuiShader;
    ShaderProgram mGrassShader;
    ShaderProgram mGBufferShader;
    ShaderProgram mSkinningShader;
    ShaderProgram mLightingShader;
    ShaderProgram mLightCullShader;
    ShaderProgram mDownsampleShader;
    ShaderProgram mTAAResolveShader;
    ShaderProgram mFinalComposeShader;
    ShaderProgram mDebugPrimitivesShader;

    ShaderProgram mSkyCubeShader;
    ShaderProgram mConvolveCubeShader;

    ShaderProgram mProbeDebugShader;
    ShaderProgram mProbeDebugRaysShader;

    ShaderProgram mProbeTraceShader;
    ShaderProgram mProbeSampleShader;
    ShaderProgram mProbeUpdateDepthShader;
    ShaderProgram mProbeUpdateIrradianceShader;

    ShaderProgram mRTReflectionsShader;
    ShaderProgram mRTPathTraceShader;
    ShaderProgram mRTAmbientOcclusionShader;

    ShaderProgram mTraceShadowRaysShader;
    ShaderProgram mClearShadowTilesShader;
    ShaderProgram mClassifyShadowTilesShader;
    ShaderProgram mDenoiseShadowTilesShader;

    ShaderProgram mGBufferDebugDepthShader;
    ShaderProgram mGBufferDebugAlbedoShader;
    ShaderProgram mGBufferDebugNormalsShader;
    ShaderProgram mGBufferDebugEmissiveShader;
    ShaderProgram mGBufferDebugVelocityShader;
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
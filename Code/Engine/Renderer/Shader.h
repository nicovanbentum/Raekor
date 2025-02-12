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

enum EShaderType
{
    SHADER_TYPE_VERTEX,
    SHADER_TYPE_HULL,
    SHADER_TYPE_DOMAIN,
    SHADER_TYPE_PIXEL,
    SHADER_TYPE_COMPUTE,
    SHADER_TYPE_COUNT
};

class IResource
{
    RTTI_DECLARE_VIRTUAL_TYPE(IResource);

public:
    virtual bool OnCompile(Device& inDevice) { return true; }
    virtual bool OnHotLoad(Device& inDevice) { return false; }
    virtual bool IsCompiled() const { return true; }
};


class Shader
{
    RTTI_DECLARE_VIRTUAL_TYPE(Shader);
    friend class ComputeProgram;
    friend class GraphicsProgram;

public:
    bool OnCompile(Device& inDevice, EShaderType inType, const String& inProgramDefines);

    bool IsCompiled() const { return !mBinary.empty(); }
    bool HasFilePath() const { return !mFilePath.empty(); }
    bool IsOutOfDate() const;

    uint64_t GetHash() const { return mHash; }
    const Path& GetFilePath() const { return mFilePath; }
    fs::file_time_type GetFileTime() const { return mFileTime; }
    D3D12_SHADER_BYTECODE GetShaderByteCode() const { return CD3DX12_SHADER_BYTECODE(mBinary.data(), mBinary.size()); }

private:
    Path mFilePath; // JSON
    String mDefines; // JSON
    uint64_t mHash; // BINARY
    Array<uint8_t> mBinary; // BINARY
    fs::file_time_type mFileTime; // RUNTIME
};


class GraphicsProgram : public IResource
{
    RTTI_DECLARE_VIRTUAL_TYPE(GraphicsProgram);
    friend class SystemShadersDX12;

public:
    const Shader& GetVertexShader() const { return m_VertexShader; }
    const Shader& GetHullShader() const { return m_HullShader; }
    const Shader& GetDomainShader() const { return m_DomainShader; }
    const Shader& GetPixelShader() const { return m_PixelShader; }

    bool OnCompile(Device& inDevice) override;
    bool OnHotLoad(Device& inDevice) override;
    bool IsCompiled() const override { return m_VertexShader.IsCompiled() && m_PixelShader.IsCompiled(); }

private:
    String m_Defines;
    Shader m_VertexShader, m_PixelShader;
    Shader m_HullShader, m_DomainShader;
};

class ComputeProgram : public IResource
{
    RTTI_DECLARE_VIRTUAL_TYPE(ComputeProgram);
    friend class SystemShadersDX12;

public:
    const Shader& GetComputeShader() const { return m_ComputeShader; }

    bool CompilePSO(Device& inDevice, const char* inDebugName = nullptr);
    ID3D12PipelineState* GetComputePSO() { return m_ComputePipeline.Get(); }

    bool OnCompile(Device& inDevice) override;
    bool OnHotLoad(Device& inDevice) override;
    bool IsCompiled() const override { return m_ComputeShader.IsCompiled(); }

private:
    String m_Defines;
    Shader m_ComputeShader;
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

    ComputeProgram mClearBufferShader;
    ComputeProgram mClearTexture2DShader;
    ComputeProgram mClearTexture3DShader;
    ComputeProgram mClearTextureCubeShader;

    GraphicsProgram mImGuiShader;
    GraphicsProgram mSDFUIShader;
    GraphicsProgram mGrassShader;
    GraphicsProgram mGBufferShader;
    ComputeProgram  mSkinningShader;
    GraphicsProgram mLightingShader;
    ComputeProgram  mLightCullShader;
    GraphicsProgram mShadowMapShader;
    ComputeProgram  mDownsampleShader;
    GraphicsProgram mTAAResolveShader;
    GraphicsProgram mFinalComposeShader;
    GraphicsProgram mDebugPrimitivesShader;

    ComputeProgram mSSRTraceShader;
    ComputeProgram mSSAOTraceShader;

    ComputeProgram mSkyCubeShader;
    ComputeProgram mConvolveCubeShader;

    GraphicsProgram mProbeDebugShader;
    GraphicsProgram mProbeDebugRaysShader;

    ComputeProgram mProbeTraceShader;
    ComputeProgram mProbeSampleShader;
    ComputeProgram mProbeUpdateShader;
    ComputeProgram mProbeUpdateDepthShader;
    ComputeProgram mProbeUpdateIrradianceShader;

    ComputeProgram mRTReflectionsShader;
    ComputeProgram mRTPathTraceShader;
    ComputeProgram mRTAmbientOcclusionShader;

    ComputeProgram mTraceShadowRaysShader;
    ComputeProgram mClearShadowTilesShader;
    ComputeProgram mClassifyShadowTilesShader;
    ComputeProgram mDenoiseShadowTilesShader;

    GraphicsProgram mGBufferDebugDepthShader;
    GraphicsProgram mGBufferDebugAlbedoShader;
    GraphicsProgram mGBufferDebugNormalsShader;
    GraphicsProgram mGBufferDebugEmissiveShader;
    GraphicsProgram mGBufferDebugVelocityShader;
    GraphicsProgram mGBufferDebugMetallicShader;
    GraphicsProgram mGBufferDebugRoughnessShader;

    ComputeProgram mDepthOfFieldShader;
    ComputeProgram mBloomUpSampleShader;
    ComputeProgram mBloomDownsampleShader;

    bool OnHotLoad(Device& inDevice) override;
    bool OnCompile(Device& inDevice) override;
    bool IsCompiled() const override;
};

} // Raekor::DX12
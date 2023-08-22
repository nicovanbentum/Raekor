#pragma once

namespace Raekor::DX12 {

class ShaderCompiler;
class SystemShadersDX12;

extern ShaderCompiler g_ShaderCompiler;
extern SystemShadersDX12 g_SystemShaders;


enum EShaderProgramType {
    SHADER_PROGRAM_INVALID,
    SHADER_PROGRAM_GRAPHICS,
    SHADER_PROGRAM_COMPUTE
};
RTTI_ENUM_HEADER(EShaderProgramType);
RTTI_ENUM_STRING_CONVERSIONS(EShaderProgramType);


enum EShaderType {
    SHADER_TYPE_VERTEX,
    SHADER_TYPE_PIXEL,
    SHADER_TYPE_COMPUTE
};

class IResource {
    RTTI_CLASS_HEADER(IResource);

public:
    virtual bool OnCompile() { return true; }
    virtual bool IsCompiled() const { return true; }
};


class ShaderProgram : public IResource {
    RTTI_CLASS_HEADER(ShaderProgram);
    friend class SystemShadersDX12;

public:
    bool GetGraphicsProgram(CD3DX12_SHADER_BYTECODE& ioVertexShaderByteCode, CD3DX12_SHADER_BYTECODE& ioPixelShaderByteCode) const;
    bool GetComputeProgram(CD3DX12_SHADER_BYTECODE& ioComputeShaderByteCode) const;

    EShaderProgramType GetProgramType() const { return mProgramType; }

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
    std::vector<unsigned char> mVertexShader;
    std::vector<unsigned char> mPixelShader;
    std::vector<unsigned char> mComputeShader;

    // runtime fields
    fs::file_time_type mVertexShaderFileTime;
    fs::file_time_type mPixelShaderFileTime;
    fs::file_time_type mComputeShaderFileTime;
};


class ShaderCompiler {
public:
    ComPtr<IDxcBlob> CompileShader(const Path& inPath, EShaderType inShaderType, const std::string& inDefines);
    
    void EnableShaderCache() { m_EnableCache.store(true); }
    void DisableShaderCache() { m_EnableCache.store(false); }
    
private:
    std::mutex m_ShaderCompilationMutex;
    std::atomic_bool m_EnableCache = true;
    std::unordered_map<uint64_t, ComPtr<IDxcBlob>> m_ShaderCache;
};


struct SystemShadersDX12 : public IResource {
    RTTI_CLASS_HEADER(SystemShadersDX12);

    ShaderProgram mImGuiShader;
    ShaderProgram mGrassShader;
    ShaderProgram mGBufferShader;
    ShaderProgram mLightingShader;
    ShaderProgram mDownsampleShader;
    ShaderProgram mDebugLinesShader;
    ShaderProgram mFinalComposeShader;

    ShaderProgram mProbeDebugShader;
    ShaderProgram mProbeTraceShader;
    ShaderProgram mProbeUpdateDepthShader;
    ShaderProgram mProbeUpdateIrradianceShader;

    ShaderProgram mRTShadowsShader;
    ShaderProgram mRTReflectionsShader;
    ShaderProgram mRTIndirectDiffuseShader;
    ShaderProgram mRTAmbientOcclusionShader;

    ShaderProgram mGBufferDebugDepthShader;
    ShaderProgram mGBufferDebugAlbedoShader;
    ShaderProgram mGBufferDebugNormalsShader;
    ShaderProgram mGBufferDebugMetallicShader;
    ShaderProgram mGBufferDebugRoughnessShader;

    bool HotLoad();
    bool OnCompile() override;
    bool IsCompiled() const override;
};



} // Raekor::DX12
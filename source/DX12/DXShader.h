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


enum EShaderType {
    SHADER_TYPE_VERTEX,
    SHADER_TYPE_PIXEL,
    SHADER_TYPE_COMPUTE
};


class ShaderProgram {
    RTTI_CLASS_HEADER(ShaderProgram);
    friend class ShaderCompiler;
public:
    bool GetGraphicsProgram(CD3DX12_SHADER_BYTECODE& ioVertexShaderByteCode, CD3DX12_SHADER_BYTECODE& ioPixelShaderByteCode) const;
    bool GetComputeProgram(CD3DX12_SHADER_BYTECODE& ioComputeShaderByteCode) const;

    bool IsCompiled() const;
    EShaderProgramType GetProgramType() const { return mProgramType; }

private:
    // JSON fields
    std::string mDefines = "";
    Path mVertexShaderFilePath;
    Path mPixelShaderFilePath;
    Path mComputeShaderFilePath;

    // Binary fields
    EShaderProgramType mProgramType;
    ComPtr<IDxcBlob> mVertexShader = nullptr;
    ComPtr<IDxcBlob> mPixelShader = nullptr;
    ComPtr<IDxcBlob> mComputeShader = nullptr;
};


class ShaderCompiler {
public:
    bool CompileShaderProgram(ShaderProgram& inShaderProgram);

private:
    ComPtr<IDxcBlob> CompileShader(const Path& inPath, EShaderType inShaderType, const std::string& inDefines);

    std::mutex m_ShaderCompilationMutex;
    std::unordered_map<std::string, ComPtr<IDxcBlob>> m_ShaderCache;
};


struct SystemShadersDX12 {
    RTTI_CLASS_HEADER(SystemShadersDX12);
    friend class ShaderCompiler;

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

    void CompileShaders();
};



} // Raekor::DX12
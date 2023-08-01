#pragma once

namespace Raekor::DX12 {

class SystemShadersDX12;
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
    // Serialized fields
    std::string mDefine;
    Path mVertexShaderFilePath;
    Path mPixelShaderFilePath;
    Path mComputeShaderFilePath;

    // Runtime fields
    EShaderProgramType mProgramType;
    ComPtr<IDxcBlob> mVertexShader = nullptr;
    ComPtr<IDxcBlob> mPixelShader = nullptr;
    ComPtr<IDxcBlob> mComputeShader = nullptr;
};

struct SystemShadersDX12 {
    RTTI_CLASS_HEADER(SystemShadersDX12);
    friend class ShaderCompiler;

    ShaderProgram mGBufferDebugDepthShader;
    ShaderProgram mGBufferDebugAlbedoShader;
    ShaderProgram mGBufferDebugNormalsShader;
    ShaderProgram mGBufferDebugMetallicShader;
    ShaderProgram mGBufferDebugRoughnessShader;

    void CompileShaders();
};


class ShaderCompiler {
public:
    static bool CompileShaderProgram(ShaderProgram& inShaderProgram);

private:
    static ComPtr<IDxcBlob> CompileShader(const Path& inPath, EShaderType inShaderType, const std::string& inDefines);
};

} // Raekor::DX12
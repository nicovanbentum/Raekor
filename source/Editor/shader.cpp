#include "pch.h"
#include "Raekor/util.h"
#include "shader.h"
#include "renderer.h"

namespace Raekor {

GLShader::~GLShader()
{
    glDeleteProgram(programID);
}



void GLShader::Compile(const std::initializer_list<Stage>& list)
{
    stages = list;
    Compile();
}



void GLShader::Compile()
{
    auto newProgramID = glCreateProgram();
    bool failed = false;

    std::vector<GLuint> shaders;

    for (const auto& stage : stages)
    {
        std::ifstream file(stage.binfile, std::ios::binary);
        if (!file.is_open())
            return;

        auto spirv = std::vector<unsigned char>(fs::file_size(stage.binfile));
        file.read((char*)&spirv[0], spirv.size());
        file.close();

        GLenum type = NULL;

        switch (stage.type)
        {
            case Type::VERTEX:
            {
                type = GL_VERTEX_SHADER;
            } break;
            case Type::FRAG:
            {
                type = GL_FRAGMENT_SHADER;
            } break;
            case Type::GEO:
            {
                type = GL_GEOMETRY_SHADER;
            } break;
            case Type::COMPUTE:
            {
                type = GL_COMPUTE_SHADER;
            } break;
        }

        unsigned int shaderID = glCreateShader(type);
        glShaderBinary(1, &shaderID, GL_SHADER_BINARY_FORMAT_SPIR_V, spirv.data(), (GLsizei)spirv.size());
        glSpecializeShader(shaderID, "main", 0, nullptr, nullptr);

        int shaderCompilationResult = GL_FALSE;
        int logMessageLength = 0;

        glGetShaderiv(shaderID, GL_COMPILE_STATUS, &shaderCompilationResult);

        if (shaderCompilationResult)
            shaders.push_back(shaderID);
        else
        {
            glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logMessageLength);

            auto error_msg = std::vector<char>(logMessageLength);
            glGetShaderInfoLog(shaderID, logMessageLength, NULL, error_msg.data());

            failed = true;
            std::cerr << error_msg.data() << '\n';
        }
    }

    if (shaders.empty())
        return;

    for (auto shader : shaders)
        glAttachShader(newProgramID, shader);

    glLinkProgram(newProgramID);

    int shaderCompilationResult = 0, logMessageLength = 0;

    glGetProgramiv(newProgramID, GL_LINK_STATUS, &shaderCompilationResult);

    if (shaderCompilationResult == GL_FALSE)
    {
        glGetProgramiv(newProgramID, GL_INFO_LOG_LENGTH, &logMessageLength);

        auto error_msg = std::vector<char>(logMessageLength);
        glGetProgramInfoLog(newProgramID, logMessageLength, NULL, error_msg.data());

        failed = true;
        std::cerr << error_msg.data() << '\n';
    }

    for (auto shader : shaders)
    {
        glDetachShader(newProgramID, shader);
        glDeleteShader(shader);
    }

    if (failed)
    {
        std::cerr << "failed to compile shader program\n";
        glDeleteProgram(newProgramID);
    }
    else
        programID = newProgramID;
}



bool GLShader::sGlslangValidator(const char* inVulkanSDK, const Path& inSrcPath, const Path& inDstPath)
{
    if (!fs::is_regular_file(inSrcPath))
        return false;

    const auto compiler = inVulkanSDK + std::string("\\Bin\\glslangValidator.exe -G ");
    const auto command = compiler + fs::absolute(inSrcPath).string() + " -o " + inDstPath.string();

    if (system(command.c_str()) != 0)
        return false;

    return true;
}



void GLShader::Bind()
{
    for (auto& stage : stages)
    {
        if (stage.watcher.WasModified())
        {
            const auto sdk = getenv("VULKAN_SDK");
            assert(sdk);

            sGlslangValidator(sdk, stage.textfile, stage.binfile);

            Compile();
        }
    }
    glUseProgram(programID);
}


void GLShader::Unbind()
{
    glUseProgram(0);
}


Shader::Stage::Stage(Type type, const Path& inSrcFile) :
    type(type),
    textfile(inSrcFile.string()),
    watcher(inSrcFile.string())
{
    if (!fs::exists(inSrcFile))
    {
        std::cerr << "file does not exist on disk\n";
        return;
    }

    auto outfile = inSrcFile.parent_path() / "bin" / inSrcFile.filename();
    outfile.replace_extension(outfile.extension().string() + ".spv");
    binfile = outfile.string().c_str();
}

} // Namespace Raekor
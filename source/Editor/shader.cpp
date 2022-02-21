#include "pch.h"
#include "Raekor/util.h"
#include "shader.h"
#include "renderer.h"

namespace Raekor {

glShader::glShader(const std::initializer_list<Stage>& list) 
    : stages(list) 
{}



glShader::~glShader() { 
    glDeleteProgram(programID); 
}



void glShader::compile(const std::initializer_list<Stage>& list) {
    stages = list;
    compile();
}



void glShader::compile() {
    auto newProgramID = glCreateProgram();
    bool failed = false;

    std::vector<GLuint> shaders;

    for (const auto& stage : stages) {
        std::ifstream file(stage.binfile, std::ios::ate | std::ios::binary);
        if (!file.is_open()) return;

        const size_t filesize = static_cast<size_t>(file.tellg());
        std::vector<unsigned char> spirv(filesize);
        file.seekg(0);
        file.read((char*)&spirv[0], filesize);
        file.close();

        GLenum type = NULL;

        switch (stage.type) {
            case Type::VERTEX: {
                type = GL_VERTEX_SHADER;
            } break;
            case Type::FRAG: {
                type = GL_FRAGMENT_SHADER;
            } break;
            case Type::GEO: {
                type = GL_GEOMETRY_SHADER;
            } break;
            case Type::COMPUTE: {
                type = GL_COMPUTE_SHADER;
            } break;
        }

        unsigned int shaderID = glCreateShader(type);
        glShaderBinary(1, &shaderID, GL_SHADER_BINARY_FORMAT_SPIR_V, spirv.data(), (GLsizei)spirv.size());
        glSpecializeShader(shaderID, "main", 0, nullptr, nullptr);

        int shaderCompilationResult = GL_FALSE;
        int logMessageLength = 0;

        glGetShaderiv(shaderID, GL_COMPILE_STATUS, &shaderCompilationResult);
        
        if (shaderCompilationResult) {

            shaders.push_back(shaderID);

        } else {

            glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logMessageLength);

            std::vector<char> error_msg(logMessageLength);

            glGetShaderInfoLog(shaderID, logMessageLength, NULL, error_msg.data());

            std::cerr << error_msg.data() << '\n';

            failed = true;
        }
    }

    if (shaders.empty()) return;

    for (auto shader : shaders) {
        glAttachShader(newProgramID, shader);
    }

    glLinkProgram(newProgramID);

    int shaderCompilationResult = 0, logMessageLength = 0;

    glGetProgramiv(newProgramID, GL_LINK_STATUS, &shaderCompilationResult);

    if (shaderCompilationResult == GL_FALSE) {
        glGetProgramiv(newProgramID, GL_INFO_LOG_LENGTH, &logMessageLength);
        
        std::vector<char> errorMessage(logMessageLength);
       
        glGetProgramInfoLog(newProgramID, logMessageLength, NULL, errorMessage.data());
        
        std::cerr << errorMessage.data() << '\n';
        
        failed = true;
    }

    for (auto shader : shaders) {
        glDetachShader(newProgramID, shader);
        glDeleteShader(shader);
    }

    if (failed) {
        std::cerr << "failed to compile shader program\n";
        glDeleteProgram(newProgramID);
    } else {
        programID = newProgramID;
    }
}



bool glShader::glslangValidator(const char* vulkanSDK, const fs::path& file, const fs::path& outfile) {
    if (!fs::is_regular_file(file)) return false;

    const auto compiler = vulkanSDK + std::string("\\Bin\\glslangValidator.exe -G ");
    const auto command = compiler + fs::absolute(file).string() + " -o " + std::string(outfile.string());

    if (system(command.c_str()) != 0) {
        return false;
    }

    return true;
}



void glShader::bind() { 
    for (auto& stage : stages) {
        if (stage.watcher.WasModified()) {
            const auto sdk = getenv("VULKAN_SDK");
            assert(sdk);

            glslangValidator(sdk, stage.textfile, stage.binfile);

            compile();
        }
    }

    glUseProgram(programID); 
}



void glShader::unbind() { 
    glUseProgram(0); 
}



Shader::Stage::Stage(Type type, const fs::path& textfile) : 
    type(type), 
    textfile(textfile.string()),
    watcher(textfile.string()) 
{
    if (!std::filesystem::exists(textfile)) {
        std::cerr << "file does not exist on disk\n";
        return;
    }

    auto outfile = textfile.parent_path() / "bin" / textfile.filename();
    outfile.replace_extension(outfile.extension().string() + ".spv");
    binfile = outfile.string().c_str();
}

} // Namespace Raekor
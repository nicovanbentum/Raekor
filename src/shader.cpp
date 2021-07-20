#include "pch.h"
#include "util.h"
#include "shader.h"
#include "renderer.h"

#ifdef _WIN32
#include "platform/windows/DXShader.h"
#endif

namespace Raekor {

Shader* Shader::construct(Stage* stages, size_t stageCount) {
    switch (Renderer::getActiveAPI()) {
        case RenderAPI::OPENGL: {
            return nullptr;

        } break;
#ifdef _WIN32
        case RenderAPI::DIRECTX11: {
            LOG_CATCH(return new DXShader(stages, stageCount));
        } break;
#endif
    }
    return nullptr;
}



glShader::glShader(const std::initializer_list<Stage>& list) : stages(list) {}



glShader::~glShader() { glDeleteProgram(programID); }

void glShader::compileSPIRV(const std::initializer_list<Stage>& list) {
    stages = list;
    compileSPIRV();
}

void glShader::compile(const std::initializer_list<Stage>& list) {
    stages = list;
    compile();
}

void glShader::compileSPIRV() {
    auto newProgramID = glCreateProgram();
    bool failed = false;

    std::vector<GLuint> shaders;

    for (const auto& stage : stages) {
        std::ifstream file(stage.filepath, std::ios::ate | std::ios::binary);
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
        if (shaderCompilationResult == GL_FALSE) {
            glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logMessageLength);
            std::vector<char> error_msg(logMessageLength);
            glGetShaderInfoLog(shaderID, logMessageLength, NULL, error_msg.data());
            std::puts(error_msg.data());
            failed = true;
        }         else {
            shaders.push_back(shaderID);
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
        std::puts(errorMessage.data());
        failed = true;
    }

    for (auto shader : shaders) {
        glDetachShader(newProgramID, shader);
        glDeleteShader(shader);
    }

    if (failed) {
        std::cerr << "failed to compile shader program" << std::endl;
        glDeleteProgram(newProgramID);
    } else {
        programID = newProgramID;
    }
}

void glShader::compile() {
    auto newProgramID = glCreateProgram();
    bool failed = false;
    
    std::vector<unsigned int> shaders;
    for (const auto& stage : stages) {

        std::string buffer;
        std::ifstream ifs(stage.filepath, std::ios::in | std::ios::binary);
        if (ifs) {
            ifs.seekg(0, std::ios::end);
            buffer.resize(ifs.tellg());
            ifs.seekg(0, std::ios::beg);
            ifs.read(&buffer[0], buffer.size());
            ifs.close();
        } else {
            std::cout << stage.filepath << " does not exist on disk." << "\n";
        }

        const char* src = buffer.c_str();

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
        glShaderSource(shaderID, 1, &src, NULL);
        glCompileShader(shaderID);

        int shaderCompilationResult = GL_FALSE;
        int logMessageLength = 0;

        glGetShaderiv(shaderID, GL_COMPILE_STATUS, &shaderCompilationResult);
        if (shaderCompilationResult == GL_FALSE) {
            glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logMessageLength);
            std::vector<char> error_msg(logMessageLength);
            glGetShaderInfoLog(shaderID, logMessageLength, NULL, error_msg.data());
            std::puts(error_msg.data());
            failed = true;
        }
        else {
            shaders.push_back(shaderID);
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
        std::puts(errorMessage.data());
        failed = true;
    }

    for (auto shader : shaders) {
        glDetachShader(newProgramID, shader);
        glDeleteShader(shader);
    }

    if (failed) {
        std::cerr << "failed to compile shader program" << std::endl;
        glDeleteProgram(newProgramID);
    } else {
        programID = newProgramID;
    }
}

bool glShader::glslangValidator(const char* vulkanSDK, const fs::directory_entry& file) {
    if (!file.is_regular_file()) return false;

    const auto outfile = file.path().parent_path() / "bin" / file.path().filename();
    const auto compiler = vulkanSDK + std::string("\\Bin\\glslangValidator.exe -G ");
    const auto command = compiler + file.path().string() + " -o " + std::string(outfile.string() + ".spv");

    if (system(command.c_str()) != 0) {
        return false;
    }

    return true;
}



void glShader::bind() { 
    for (auto& stage : stages) {
        if (stage.watcher.wasModified()) {
            compile();
        }
    }

    glUseProgram(programID); 
}



void glShader::unbind() { 
    glUseProgram(0); 
}



Shader::Stage::Stage(Type type, const char* filepath) : 
    type(type), 
    filepath(filepath), 
    watcher(filepath) 
{
    if (!std::filesystem::exists(filepath)) {
        std::cerr << "file does not exist on disk\n";
    }
}

} // Namespace Raekor
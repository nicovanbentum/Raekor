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
            LOG_CATCH(return new glShader(stages, stageCount));

        } break;
#ifdef _WIN32
        case RenderAPI::DIRECTX11: {
            LOG_CATCH(return new DXShader(stages, stageCount));
        } break;
#endif
    }
    return nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////

glShader::glShader(Stage* stages, size_t stageCount) {
    this->reload(stages, stageCount);
}

/////////////////////////////////////////////////////////////////////////////////////////

glShader::~glShader() { glDeleteProgram(programID); }

/////////////////////////////////////////////////////////////////////////////////////////

void glShader::reload(Stage* stages, size_t stageCount) {
    programID = glCreateProgram();
    std::vector<unsigned int> shaders;

    for (unsigned int i = 0; i < stageCount; i++) {
        Stage& stage = stages[i];

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

        auto it = buffer.find_first_of('\n') + 1;
        for (std::string& define : stage.defines) {

            buffer.insert(it, "#define " + define + '\n');
            it += 9 + define.size();
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
            throw std::runtime_error(error_msg.data());
        }

        glAttachShader(programID, shaderID);
        shaders.push_back(shaderID);
    }

    // Link and check the program
    glLinkProgram(programID);

    int shaderCompilationResult = 0, logMessageLength = 0;
    glGetProgramiv(programID, GL_LINK_STATUS, &shaderCompilationResult);
    if (shaderCompilationResult == GL_FALSE) {
        std::cout << "FAILED TO LINK GL SHADERS" << '\n';

        glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &logMessageLength);
        std::vector<char> errorMessage(logMessageLength);
        glGetProgramInfoLog(programID, logMessageLength, NULL, errorMessage.data());
        throw std::runtime_error(errorMessage.data());
    }

    for (unsigned int shader : shaders) {
        glDetachShader(programID, shader);
        glDeleteShader(shader);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////

inline const void glShader::bind() const { glUseProgram(programID); }

/////////////////////////////////////////////////////////////////////////////////////////

inline const void glShader::unbind() const { glUseProgram(0); }

/////////////////////////////////////////////////////////////////////////////////////////

glShader::UniformLocation glShader::operator[] (const char* name) {
    return { glGetUniformLocation(programID, name) };

}

glShader::UniformLocation glShader::getUniform(const char* name) {
    return { glGetUniformLocation(programID, name) };
}

/////////////////////////////////////////////////////////////////////////////////////////

glShader::UniformLocation& glShader::UniformLocation::operator=(const glm::mat4& rhs) {
    glUniformMatrix4fv(id, 1, GL_FALSE, glm::value_ptr(rhs));
    return *this;
}

glShader::UniformLocation& glShader::UniformLocation::operator=(const std::vector<glm::vec3>& rhs) {
    glUniform3fv(id, static_cast<GLsizei>(rhs.size()), glm::value_ptr(rhs[0]));
    return *this;
}

glShader::UniformLocation& glShader::UniformLocation::operator=(float rhs) {
    glUniform1f(id, rhs);
    return *this;
}

glShader::UniformLocation& glShader::UniformLocation::operator=(bool rhs) {
    glUniform1i(id, rhs);
    return *this;
}

glShader::UniformLocation& glShader::UniformLocation::operator=(const glm::vec4& rhs) {
    glUniform4f(id, rhs.x, rhs.y, rhs.z, rhs.w);
    return *this;
}

glShader::UniformLocation& glShader::UniformLocation::operator=(const glm::vec3& rhs) {
    glUniform3f(id, rhs.x, rhs.y, rhs.z);
    return *this;
}

glShader::UniformLocation& glShader::UniformLocation::operator=(const glm::vec2& rhs) {
    glUniform2f(id, rhs.x, rhs.y);
    return *this;
}

glShader::UniformLocation& glShader::UniformLocation::operator=(uint32_t rhs) {
    glUniform1i(id, rhs);
    return *this;
}

/////////////////////////////////////////////////////////////////////////////////////////

void ShaderHotloader::watch(glShader* shader, Shader::Stage* inStages, size_t stageCount) {
    // store the stages    
    for (int i = 0; i < stageCount; i++) {
        stages.push_back(inStages[i]);
    }

    // store a lambda that keeps a copy of pointers to the shader
    checks.emplace_back([=]() {
        for (auto& stage : stages) {
            if (stage.watcher.wasModified()) {
                // in case compilation fails we discard progress and assign the old shader program
                unsigned int temporaryID = shader->programID;
                try {
                    shader->reload(stages.data(), stageCount);
                } catch(std::exception e) {
                    std::puts(e.what());
                    shader->programID = temporaryID;
                }
            }
        }
        });
}

void ShaderHotloader::checkForUpdates() {
    for (auto& check : checks) {
        check();
    }
}

} // Namespace Raekor
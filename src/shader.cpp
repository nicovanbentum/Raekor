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
            LOG_CATCH(return new GLShader(stages, stageCount));

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

GLShader::GLShader(Stage* stages, size_t stageCount) {
    this->reload(stages, stageCount);
}

/////////////////////////////////////////////////////////////////////////////////////////

GLShader::~GLShader() { glDeleteProgram(programID); }

/////////////////////////////////////////////////////////////////////////////////////////

void GLShader::reload(Stage* stages, size_t stageCount) {
    programID = glCreateProgram();
    std::vector<unsigned int> shaders;

    for (unsigned int i = 0; i < stageCount; i++) {
        Stage& stage = stages[i];

        std::string buffer;
        std::ifstream ifs(stage.filepath, std::ios::in | std::ios::binary);
        if (ifs) {
            std::cout << programID << " : " << stage.filepath << std::endl;
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
            m_assert(!logMessageLength, std::string(std::begin(error_msg), std::end(error_msg)));
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
        m_assert(!logMessageLength, std::string(std::begin(errorMessage), std::end(errorMessage)));
    }

    for (unsigned int shader : shaders) {
        glDetachShader(programID, shader);
        glDeleteShader(shader);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////

inline const void GLShader::bind() const { glUseProgram(programID); }

/////////////////////////////////////////////////////////////////////////////////////////

inline const void GLShader::unbind() const { glUseProgram(0); }

/////////////////////////////////////////////////////////////////////////////////////////

GLShader::UniformLocation GLShader::operator[] (const char* name) {
    return { glGetUniformLocation(programID, name) };

}

GLShader::UniformLocation GLShader::getUniform(const char* name) {
    return { glGetUniformLocation(programID, name) };
}

/////////////////////////////////////////////////////////////////////////////////////////

GLShader::UniformLocation& GLShader::UniformLocation::operator=(const glm::mat4& rhs) {
    glUniformMatrix4fv(id, 1, GL_FALSE, glm::value_ptr(rhs));
    return *this;
}

GLShader::UniformLocation& GLShader::UniformLocation::operator=(const std::vector<glm::vec3>& rhs) {
    glUniform3fv(id, static_cast<GLsizei>(rhs.size()), glm::value_ptr(rhs[0]));
    return *this;
}

GLShader::UniformLocation& GLShader::UniformLocation::operator=(float rhs) {
    glUniform1f(id, rhs);
    return *this;
}

GLShader::UniformLocation& GLShader::UniformLocation::operator=(bool rhs) {
    glUniform1i(id, rhs);
    return *this;
}

GLShader::UniformLocation& GLShader::UniformLocation::operator=(const glm::vec4& rhs) {
    glUniform4f(id, rhs.x, rhs.y, rhs.z, rhs.w);
    return *this;
}

GLShader::UniformLocation& GLShader::UniformLocation::operator=(const glm::vec3& rhs) {
    glUniform3f(id, rhs.x, rhs.y, rhs.z);
    return *this;
}

GLShader::UniformLocation& GLShader::UniformLocation::operator=(const glm::vec2& rhs) {
    glUniform2f(id, rhs.x, rhs.y);
    return *this;
}

GLShader::UniformLocation& GLShader::UniformLocation::operator=(uint32_t rhs) {
    glUniform1i(id, rhs);
    return *this;
}

/////////////////////////////////////////////////////////////////////////////////////////

void ShaderHotloader::watch(GLShader* shader, Shader::Stage* stages, size_t stageCount) {
    // store a lambda that keeps a copy of pointers to the shader and stages
    checks.emplace_back([=]() {
        for (unsigned int i = 0; i < stageCount; i++) {
            if (stages[i].watcher.wasModified()) {
                shader->reload(stages, stageCount);
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
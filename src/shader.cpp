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

        int result = GL_FALSE;
        int log_n = 0;

        glGetShaderiv(shaderID, GL_COMPILE_STATUS, &result);
        if (result == GL_FALSE) {
            glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &log_n);
            std::vector<char> error_msg(log_n);
            glGetShaderInfoLog(shaderID, log_n, NULL, error_msg.data());
            m_assert(!log_n, std::string(std::begin(error_msg), std::end(error_msg)));
        }

        glAttachShader(programID, shaderID);
        shaders.push_back(shaderID);
    }

    // Link and check the program
    glLinkProgram(programID);

    int result = 0, log_n = 0;
    glGetProgramiv(programID, GL_LINK_STATUS, &result);
    if (result == GL_FALSE) {
        std::cout << "FAILED TO LINK GL SHADERS" << '\n';

        glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &log_n);
        std::vector<char> error_msg(log_n);
        glGetProgramInfoLog(programID, log_n, NULL, error_msg.data());
        m_assert(!log_n, std::string(std::begin(error_msg), std::end(error_msg)));
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

Shader::loc GLShader::operator[] (const char* data) {
    loc ret;
    ret.id = glGetUniformLocation(programID, data);
    return ret;
}

Shader::loc GLShader::getUniform(const char* name) {
    return { glGetUniformLocation(programID, name) };
}

} // Namespace Raekor
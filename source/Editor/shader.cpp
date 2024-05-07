#include "pch.h"
#include "shader.h"
#include "renderer.h"

namespace RK {

bool Shader::Stage::WasModified()
{
    fs::file_time_type new_time = std::filesystem::last_write_time(path);

    if (new_time != lastwrite_time)
    {
        lastwrite_time = new_time;
        return true;
    }

    return false;
}


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
    GLuint newProgramID = glCreateProgram();
    bool failed = false;

    Array<GLuint> shaders;

    for (const Stage& stage : stages)
    {
        std::ifstream file(stage.textfile, std::ios::binary);
        if (!file.is_open())
            return;

        std::ostringstream string_stream;
        string_stream << file.rdbuf();

        String shader_source = string_stream.str();
        const char* shader_source_cstr = shader_source.c_str();

        GLenum type = NULL;

        switch (stage.type)
        {
            case Type::VERTEX:   type = GL_VERTEX_SHADER;   break;
            case Type::FRAGMENT: type = GL_FRAGMENT_SHADER; break;
            case Type::GEOMETRY: type = GL_GEOMETRY_SHADER; break;
            case Type::COMPUTE:  type = GL_COMPUTE_SHADER;  break;
            default:
                assert(false);
        }

        GLuint shaderID = glCreateShader(type);
        glShaderSource(shaderID, 1, &shader_source_cstr, NULL);
        glCompileShader(shaderID);

        GLint compile_success = GL_FALSE;
        GLint error_msg_length = 0;

        glGetShaderiv(shaderID, GL_COMPILE_STATUS, &compile_success);

        if (compile_success == GL_TRUE)
            shaders.push_back(shaderID);
        else
        {
            glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &error_msg_length);

            Array<char> error_msg(error_msg_length);
            glGetShaderInfoLog(shaderID, error_msg_length, NULL, error_msg.data());

            failed = true;

            if (!error_msg.empty())
                std::cerr << error_msg.data() << '\n';
        }
    }

    assert(!shaders.empty());

    if (shaders.empty())
        return;

    for (GLuint shader : shaders)
        glAttachShader(newProgramID, shader);

    glLinkProgram(newProgramID);

    GLint link_success = GL_FALSE;
    GLint error_msg_length = 0;

    glGetProgramiv(newProgramID, GL_LINK_STATUS, &link_success);

    if (link_success == GL_FALSE)
    {
        glGetProgramiv(newProgramID, GL_INFO_LOG_LENGTH, &error_msg_length);

        Array<char> error_msg(error_msg_length);
        glGetProgramInfoLog(newProgramID, error_msg_length, NULL, error_msg.data());

        failed = true;

        if (!error_msg.empty())
            std::cerr << error_msg.data() << '\n';
    }

    for (GLuint shader : shaders)
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



void GLShader::Bind()
{
    for (Stage& stage : stages)
    {
        if (stage.WasModified())
            Compile();
    }
    glUseProgram(programID);
}


void GLShader::Unbind()
{
    glUseProgram(0);
}


Shader::Stage::Stage(Type type, const Path& inSrcFile) :
    type(type),
    path(inSrcFile),
    textfile(inSrcFile.string())
{
    lastwrite_time = fs::last_write_time(path);

    if (!fs::exists(inSrcFile))
    {
        std::cerr << "file does not exist on disk\n";
        return;
    }
}

} // Namespace RK
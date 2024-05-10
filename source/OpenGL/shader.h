#pragma once

namespace RK {

class Shader
{
public:
    enum class Type { VERTEX, FRAGMENT, GEOMETRY, COMPUTE };

    struct Stage
    {
        Stage(Type type, const Path& inSrcFile);

        bool WasModified();

        Type type;
        Path path;
        String textfile;
        fs::file_time_type lastwrite_time;
    };

    virtual void Bind() = 0;
    virtual void Unbind() = 0;
};


class GLShader : public Shader
{
public:
    ~GLShader();

    void Compile(const std::initializer_list<Stage>& list);
    void Compile();

    operator bool() { return programID != 0; };

    void Bind() override;
    void Unbind() override;

private:
    GLuint programID = 0;
    Array<Shader::Stage> stages;
};

} // Namespace Raekor

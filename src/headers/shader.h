#pragma once

#include "pch.h"
#include "util.h"

namespace Raekor {

/////////////////////////////////////////////////////////////////////

class Shader {
public:
    enum class Type { VERTEX, FRAG, GEO, COMPUTE };

    struct Stage {
        Type type;
        const char* filepath;
        FileWatcher watcher;
        std::vector<std::string> defines;

        Stage(Type type, const char* filepath) : type(type), filepath(filepath), watcher(filepath) {}
    };


    static Shader* construct(Stage* stages, size_t stageCount);
    virtual const void bind() const = 0;
    virtual const void unbind() const = 0;
};

/////////////////////////////////////////////////////////////////////

class GLShader : public Shader {
    struct UniformLocation {
        GLint id;

        UniformLocation& operator=(const glm::mat4& rhs);
        UniformLocation& operator=(const std::vector<glm::vec3>& rhs);
        UniformLocation& operator=(float rhs);
        UniformLocation& operator=(bool rhs);
        UniformLocation& operator=(const glm::vec4& rhs);
        UniformLocation& operator=(const glm::vec3& rhs);
        UniformLocation& operator=(const glm::vec2& rhs);
        UniformLocation& operator=(uint32_t rhs);
    };

public:
    GLShader() {}
    GLShader(Stage* stages, size_t stageCount);
    ~GLShader();

    void reload(Stage* stages, size_t stageCount);

    inline const void bind() const;
    inline const void unbind() const;

    inline unsigned int getID() const { return programID; }

    UniformLocation operator[] (const char* data);
    UniformLocation getUniform(const char* name);

private:
    unsigned int programID;
};

class ShaderHotloader {
public:
    void watch(GLShader* shader, Shader::Stage* stages, size_t stageCount);
    void checkForUpdates();

private:
    std::vector<std::function<void()>> checks;
};

} // Namespace Raekor

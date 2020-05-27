#pragma once

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

class glShader : public Shader {
    struct UniformLocation {
        GLint id;

        UniformLocation& operator=(bool rhs);
        UniformLocation& operator=(float rhs);
        UniformLocation& operator=(uint32_t rhs);
        UniformLocation& operator=(const glm::vec2& rhs);
        UniformLocation& operator=(const glm::vec3& rhs);
        UniformLocation& operator=(const glm::vec4& rhs);
        UniformLocation& operator=(const glm::mat4& rhs);
        UniformLocation& operator=(const std::vector<glm::vec3>& rhs);
    };

public:
    glShader() {}
    glShader(Stage* stages, size_t stageCount);
    ~glShader();

    void reload(Stage* stages, size_t stageCount);

    inline const void bind() const;
    inline const void unbind() const;

    UniformLocation operator[] (const char* data);
    UniformLocation getUniform(const char* name);

    unsigned int programID;
};

class ShaderHotloader {
public:
    void watch(glShader* shader, Shader::Stage* stages, size_t stageCount);
    void checkForUpdates();

private:
    std::vector<Shader::Stage> stages;
    std::vector<std::function<void()>> checks;
};

} // Namespace Raekor

#pragma once

#include "util.h"

namespace Raekor {

//////////////////////////////////////////////////////////////////////////////////////////////////

class Shader {
public:
    enum class Type { VERTEX, FRAG, GEO, COMPUTE };

    struct Stage {
        Type type;
        const char* filepath;
        FileWatcher watcher;
        std::vector<std::string> defines;

        Stage(Type type, const char* filepath) : type(type), filepath(filepath), watcher(filepath) {
            if (!std::filesystem::exists(filepath)) {
                std::cerr << "file does not exist on disk\n";
            }
        }
    };


    static Shader* construct(Stage* stages, size_t stageCount);
    virtual const void bind() const = 0;
    virtual const void unbind() const = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

struct UniformLocation {
    GLint id;

    UniformLocation& operator=(bool rhs);
    UniformLocation& operator=(float rhs);
    UniformLocation& operator=(int32_t rhs);
    UniformLocation& operator=(uint32_t rhs);
    UniformLocation& operator=(const glm::vec2& rhs);
    UniformLocation& operator=(const glm::vec3& rhs);
    UniformLocation& operator=(const glm::vec4& rhs);
    UniformLocation& operator=(const glm::mat4& rhs);
    UniformLocation& operator=(const std::vector<float>& rhs);
    UniformLocation& operator=(const std::vector<glm::vec3>& rhs);
    UniformLocation& operator=(const std::vector<glm::mat4>& rhs);
};

class glShader : public Shader {

public:
    glShader() = default;
    glShader(Stage* stages, size_t stageCount);
    ~glShader();

    void reload(Stage* stages, size_t stageCount);

    operator bool() { return programID != 0; };

    inline const void bind() const;
    inline const void unbind() const;

    UniformLocation operator[] (const char* data);
    UniformLocation getUniform(const char* name);

    unsigned int programID = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class ShaderHotloader {
public:
    void watch(glShader* shader, Shader::Stage* stages, size_t stageCount);
    bool changed();

private:
    std::vector<Shader::Stage> stages;
    std::vector<std::function<bool()>> checks;
};

} // Namespace Raekor

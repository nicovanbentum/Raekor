#pragma once

#include "util.h"

namespace Raekor {

class Shader {
public:
    enum class Type { VERTEX, FRAG, GEO, COMPUTE };

    struct Stage {
        Stage(Type type, const char* filepath);

        Type type;
        const char* filepath;
        FileWatcher watcher;
    };

    static Shader* construct(Stage* stages, size_t stageCount);
    virtual void bind() = 0;
    virtual void unbind() = 0;
};

class glShader : public Shader {

public:
    glShader() = default;
    glShader(const std::initializer_list<Stage>& list);
    ~glShader();

    void compile(const std::initializer_list<Stage>& list);
    void compile();

    operator bool() { return programID != 0; };

    void bind() override;
    void unbind() override;

    unsigned int programID = 0;
    std::vector<Shader::Stage> stages;
};

} // Namespace Raekor

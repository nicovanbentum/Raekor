#pragma once

#include "util.h"

namespace Raekor {

class Shader {
public:
    enum class Type { VERTEX, FRAG, GEO, COMPUTE };

    struct Stage {
        Stage(Type type, const fs::path& textfile);

        Type type;
        std::string textfile;
        std::string binfile;
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

    static bool glslangValidator(const char* vulkanSDK, const fs::path& file, const fs::path& outfile);

    operator bool() { return programID != 0; };

    void bind() override;
    void unbind() override;

    unsigned int programID = 0;
    std::vector<Shader::Stage> stages;
};

} // Namespace Raekor

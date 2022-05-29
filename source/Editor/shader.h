#pragma once

#include "Raekor/util.h"

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

    virtual void Bind() = 0;
    virtual void Unbind() = 0;
};


class glShader : public Shader {
public:
    glShader() = default;
    glShader(const std::initializer_list<Stage>& list);
    ~glShader();

    void Compile(const std::initializer_list<Stage>& list);
    void Compile();

    static bool sGlslangValidator(const char* vulkanSDK, const fs::path& file, const fs::path& outfile);

    operator bool() { return programID != 0; };

    void Bind() override;
    void Unbind() override;

private:
    GLuint programID = 0;
    std::vector<Shader::Stage> stages;
};

} // Namespace Raekor

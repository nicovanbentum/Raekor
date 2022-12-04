#pragma once

#include "Raekor/util.h"

namespace Raekor {

class Shader {
public:
    enum class Type { VERTEX, FRAG, GEO, COMPUTE };

    struct Stage {
        Stage(Type type, const Path& inSrcFile);

        Type type;
        std::string textfile;
        std::string binfile;
        FileWatcher watcher;
    };

    virtual void Bind() = 0;
    virtual void Unbind() = 0;
};


class GLShader : public Shader {
public:
    ~GLShader();

    void Compile(const std::initializer_list<Stage>& list);
    void Compile();

    static bool sGlslangValidator(const char* inVulkanSDK, const Path& inSrcPath, const Path& inDstPath);

    operator bool() { return programID != 0; };

    void Bind() override;
    void Unbind() override;

private:
    GLuint programID = 0;
    std::vector<Shader::Stage> stages;
};

} // Namespace Raekor

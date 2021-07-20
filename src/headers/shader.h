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

    void compileSPIRV(const std::initializer_list<Stage>& list);
    void compileSPIRV();

    void compile(const std::initializer_list<Stage>& list);
    void compile();

    static bool glslangValidator(const char* vulkanSDK, const fs::directory_entry& file) {
        if (!file.is_regular_file()) return false;;

        const auto outfile = file.path().parent_path() / "bin" / file.path().filename();
        const auto compiler = vulkanSDK + std::string("\\Bin\\glslangValidator.exe -G ");
        const auto command = compiler + file.path().string() + " -o " + std::string(outfile.string() + ".spv");

        if (system(command.c_str()) != 0) {
            return false;
        }

        return true;
    }

    operator bool() { return programID != 0; };

    void bind() override;
    void unbind() override;

    unsigned int programID = 0;
    std::vector<Shader::Stage> stages;
};

} // Namespace Raekor

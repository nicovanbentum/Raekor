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

        Stage(Type type, const char* filepath) : type(type), filepath(filepath) {}
    };


    static Shader* construct(Stage* stages, size_t stageCount);
    virtual const void bind() const = 0;
    virtual const void unbind() const = 0;

protected:
    struct loc {
        unsigned int id;
        loc& operator=(const void* rhs);
    };
};

/////////////////////////////////////////////////////////////////////

class GLShader : public Shader {
public:
    GLShader(Stage* stages, size_t stageCount);
    ~GLShader();

    void reload(Stage* stages, size_t stageCount);

    inline const void bind() const;
    inline const void unbind() const;

    loc operator[] (const char* data);

private:
    unsigned int programID;
};

} // Namespace Raekor

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

        template<typename T>
        loc& operator=(T rhs) {
            return *this;
        }

        template<>
        loc& operator=(const glm::mat4& rhs) {
            glUniformMatrix4fv(id, 1, GL_FALSE, glm::value_ptr(rhs));
            return *this;
        }

        template<>
        loc& operator=(float rhs) {
            glUniform1f(id, rhs);
            return *this;
        }

        template<>
        loc& operator=(bool rhs) {
            glUniform1i(id, rhs);
        }

        template<>
        loc& operator=(const glm::vec4& rhs) {
            glUniform4f(id, rhs.x, rhs.y, rhs.z, rhs.w);
        }
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

    inline unsigned int getID() const { return programID; }

    loc operator[] (const char* data);

private:
    unsigned int programID;
};

} // Namespace Raekor

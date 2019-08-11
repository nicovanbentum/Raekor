#pragma once

#include "pch.h"
#include "util.h"

namespace Raekor {

struct loc {
    unsigned int id;

    loc& operator=(const void* rhs) {
        glUniformMatrix4fv(id, 1, GL_FALSE, (float*)rhs);
        return *this;
    }
};

class Shader {
public:
    static Shader* construct(std::string vertex, std::string fp);
    virtual const void bind() const = 0;
    virtual const void unbind() const = 0;
};

class GLShader : public Shader {
public:
    ~GLShader() { glDeleteProgram(id); }
    GLShader(std::string frag, std::string vert);

    const void bind() const override { glUseProgram(id); }
    const void unbind() const override { glUseProgram(0); }

    loc operator[] (const char* data) {
        loc ret;
        ret.id = glGetUniformLocation(id, data);
        return ret;
    }

    inline unsigned int get_id() const { return id; }

    unsigned int get_uniform_location(const std::string& var);
    void upload_uniform_matrix4fv(unsigned int var_id, float* start);

protected:
    unsigned int id;
};

} // Namespace Raekor

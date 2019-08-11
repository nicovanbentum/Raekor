#pragma once

#include "pch.h"

// message assert macro
#ifndef NDEBUG
    #define m_assert(expr, msg) assert((msg, expr))
#else 
    #define m_assert(expr, msg) (void)(expr)
#endif

// alias for Microsoft's com pointers
template<typename T>
using com_ptr = Microsoft::WRL::ComPtr<T>;

namespace Raekor {

// function to create an OpenGL buffer out of a vector of whatever type
template<typename T>
unsigned int gen_gl_buffer(const std::vector<T> & v, GLenum target) {
    unsigned int buffer_id;
    glGenBuffers(1, &buffer_id);
    glBindBuffer(target, buffer_id);
    glBufferData(target, v.size() * sizeof(T), &v[0], GL_STATIC_DRAW);
    glBindBuffer(target, 0);
    return buffer_id;
}

// TODO: this doesn't actually return the extension, but the name + plus extension
static std::string get_extension(const std::string& path) {
    std::string filename = "";

    if (!path.empty()) {
        for(int i = (int)path.size()-1; i > 0; i--) {
            if (path[i] == '\\' || path[i] == '/') {
                return filename;
            } filename.insert(0, std::string(1, path[i]));
        }
    }
    return std::string();
}

} // Namespace Raekor

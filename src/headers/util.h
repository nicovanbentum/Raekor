#pragma once

#include "pch.h"

#ifndef NDEBUG
    #define m_assert(expr, msg) assert((msg, expr))
#else 
    #define m_assert(expr, msg) expr
#endif

template<typename T>
T jfind(json & j, const char * value) {
    m_assert(j.find(value) != j.end(), "value doesn't exist");
    return (*j.find(value)).get<T>();
}

template<typename T>
unsigned int gen_gl_buffer(std::vector<T> & v, GLenum target) {
    unsigned int buffer_id;
    glGenBuffers(1, &buffer_id);
    glBindBuffer(target, buffer_id);
    glBufferData(target, v.size() * sizeof(T), &v[0], GL_STATIC_DRAW);
    return buffer_id;
}
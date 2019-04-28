#pragma once

#include "pch.h"

inline void error(int line, const char* file, const char* function, const char * msg = nullptr)
{
    std::cout << "Error in function '" << function << "' at line " << line << " in file '" << file << "'" << std::endl;
    std::cout << "Trace message: ";
    std::cout << msg << std::endl;
    std::cout << "Press Enter to conintue..";
    std::cin.get();
} 
#define trace(x) error(__LINE__, __FILE__, __FUNCTION__, x)

template<typename T>
T jfind(json & j, const char * value) {
    if(j.find(value) == j.end()) trace("value doesn't exist");
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
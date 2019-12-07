#pragma once

#include "pch.h"

// message assert macro
#ifndef NDEBUG
    #define m_assert(expr, msg) if(!expr) std::cout << msg << std::endl; assert(expr);
#else 
    #define m_assert(expr, msg)
#endif


// alias for Microsoft's com pointers
#ifdef _WIN32
template<typename T>
using com_ptr = Microsoft::WRL::ComPtr<T>;
#endif

#define LOG_CATCH(code) try { code; } catch(std::exception e) { std::cout << e.what() << '\n'; }

namespace Raekor {

// function to create an OpenGL buffer out of a vector of whatever type
template<typename T>
unsigned int gen_gl_buffer(const std::vector<T> & v, GLenum target) {
    unsigned int buffer_id;
    glGenBuffers(1, &buffer_id);
    glBindBuffer(target, buffer_id);
    glBufferData(target, v.size() * sizeof(T), v.data(), GL_STATIC_DRAW);
    glBindBuffer(target, 0);
    return buffer_id;
}

enum class PATH_OPTIONS {
    DIR,
    FILENAME,
    EXTENSION,
    FILENAME_AND_EXTENSION
};

static std::string get_file(const std::string& path, PATH_OPTIONS option) {
    std::ifstream valid(path);
    m_assert(valid, "invalid path string passed");
    // find the last slash character index
    auto backslash = path.rfind('\\');
    auto fwdslash = path.rfind('/');
    size_t last_slash;
    if (backslash == std::string::npos) last_slash = fwdslash;
    else if (fwdslash == std::string::npos) last_slash = backslash;
    else last_slash = std::max(backslash, fwdslash);

    auto dot = path.find('.');

    switch(option) {
    case PATH_OPTIONS::DIR: return path.substr(0, last_slash+1);
    case PATH_OPTIONS::FILENAME: return path.substr(last_slash+1, (dot - last_slash) - 1);
    case PATH_OPTIONS::EXTENSION: return path.substr(dot);
    case PATH_OPTIONS::FILENAME_AND_EXTENSION: return path.substr(last_slash+1);
    default: return {};
    }
}

static void print_progress_bar(int val, int min, int max) {
    static std::string bar = "----------]";
    std::string loading = "Loading textures: [";
    auto index = (val - min) * (10 - 0) / (max - min) + 0;
    if (index == 0) bar = "----------]";
    if (bar[index] == '-') bar[index] = '#';
    std::cout << '\r' << loading << bar;
    if (val == max) std::cout << std::endl;
};


} // Namespace Raekor

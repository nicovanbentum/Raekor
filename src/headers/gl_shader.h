#pragma once

#include "pch.h"

unsigned int load_shaders(const char * vertex_file_path, const char * fragment_file_path) {
    // Create the shaders
    unsigned int vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
    unsigned int fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

    // Read the Vertex Shader code from the file
    std::string vertex_code;
    std::ifstream vertex_stream(vertex_file_path, std::ios::in);
    if (vertex_stream.is_open()) {
        std::stringstream sstr;
        sstr << vertex_stream.rdbuf();
        vertex_code = sstr.str();
        vertex_stream.close();
    }
    else {
        std::cout << "path: " << vertex_file_path << "does not exist" << std::endl;
        return 0;
    }

    // Read the Fragment Shader code from the file
    std::string fragment_code;
    std::ifstream fragment_stream(fragment_file_path, std::ios::in);
    if (fragment_stream.is_open()) {
        std::stringstream sstr;
        sstr << fragment_stream.rdbuf();
        fragment_code = sstr.str();
        fragment_stream.close();
    }

    int result = GL_FALSE;
    int log_length;

    // Compile Vertex Shader
    std::cout << "Compiling vertex shader : " << vertex_file_path << std::endl;
    char const * vertex_src = vertex_code.c_str();
    glShaderSource(vertex_shader_id, 1, &vertex_src, NULL);
    glCompileShader(vertex_shader_id);

    // Check Vertex Shader
    glGetShaderiv(vertex_shader_id, GL_COMPILE_STATUS, &result);
    glGetShaderiv(vertex_shader_id, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0) {
        std::vector<char> error_msg(log_length + 1);
        glGetShaderInfoLog(vertex_shader_id, log_length, NULL, error_msg.data());
        std::cout << std::string(std::begin(error_msg), std::end(error_msg)) << std::endl;
    }

    // Compile Fragment Shader
    printf("Compiling fragment shader : %s\n", fragment_file_path);
    char const * fragment_source = fragment_code.c_str();
    glShaderSource(fragment_shader_id, 1, &fragment_source, NULL);
    glCompileShader(fragment_shader_id);

    // Check Fragment Shader
    glGetShaderiv(fragment_shader_id, GL_COMPILE_STATUS, &result);
    glGetShaderiv(fragment_shader_id, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0) {
        std::vector<char> error_msg(log_length + 1);
        glGetShaderInfoLog(fragment_shader_id, log_length, NULL, error_msg.data());
        std::cout << std::string(std::begin(error_msg), std::end(error_msg)) << std::endl;
    }

    // Link the program
    std::cout << "Linking shader program.." << std::endl;
    unsigned int program_id = glCreateProgram();
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);
    glLinkProgram(program_id);

    // Check the program
    glGetProgramiv(program_id, GL_LINK_STATUS, &result);
    glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0) {
        std::vector<char> error_msg(log_length + 1);
        glGetProgramInfoLog(program_id, log_length, NULL, error_msg.data());
        std::cout << std::string(std::begin(error_msg), std::end(error_msg)) << std::endl;
    }

    glDetachShader(program_id, vertex_shader_id);
    glDetachShader(program_id, fragment_shader_id);

    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    return program_id;
}
#include "pch.h"
#include "util.h"
#include "shader.h"
#include "renderer.h"

#ifdef _WIN32
#include "platform/windows/DXShader.h"
#endif

namespace Raekor {

Shader* Shader::construct(std::string fp, std::string vertex) {
	switch (Renderer::get_activeAPI()) {
		case RenderAPI::OPENGL: {
			return new GLShader(fp, vertex);
		} break;
#ifdef _WIN32
		case RenderAPI::DIRECTX11: {
			return new DXShader(fp, vertex);
		} break;
#endif
	}
	return nullptr;
}

std::string read_shader_file(const std::string& fp) {
	std::string src;
	std::ifstream ifs(fp, std::ios::in | std::ios::binary);
	if(ifs) {
		ifs.seekg(0, std::ios::end);
		src.resize(ifs.tellg());
		ifs.seekg(0, std::ios::beg);
		ifs.read(&src[0], src.size());
		ifs.close();
	}
	return src;
}

GLShader::GLShader(std::string vert, std::string frag) {
	// Create the vert and frag shaders and read the code from file
	unsigned int vertex_id = glCreateShader(GL_VERTEX_SHADER);
	unsigned int frag_id = glCreateShader(GL_FRAGMENT_SHADER);
	std::string vertex_src = read_shader_file(vert);
	std::string frag_src = read_shader_file(frag);
	auto v_src = vertex_src.c_str();
	auto f_src = frag_src.c_str();

	m_assert(!vertex_src.empty() && !frag_src.empty(), "failed to load shaders");

	int result = GL_FALSE;
	int log_n = 0;

	// compile the vertex shader
	glShaderSource(vertex_id, 1, &v_src, NULL);
	glCompileShader(vertex_id);

	// Check compile completion
	glGetShaderiv(vertex_id, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		glGetShaderiv(vertex_id, GL_INFO_LOG_LENGTH, &log_n);
		std::vector<char> error_msg(log_n);
		glGetShaderInfoLog(vertex_id, log_n, NULL, error_msg.data());
		m_assert(!log_n, std::string(std::begin(error_msg), std::end(error_msg)));
	}

	// compile our fragment shader
	glShaderSource(frag_id, 1, &f_src, NULL);
	glCompileShader(frag_id);

	// check compile completion
	glGetShaderiv(frag_id, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		glGetShaderiv(frag_id, GL_INFO_LOG_LENGTH, &log_n);
		std::vector<char> error_msg(log_n);
		glGetShaderInfoLog(frag_id, log_n, NULL, error_msg.data());
		m_assert(!log_n, std::string(std::begin(error_msg), std::end(error_msg)));
	}

	// Link the program
	id = glCreateProgram();
	glAttachShader(id, vertex_id);
	glAttachShader(id, frag_id);
	glLinkProgram(id);

	// Check the program
	glGetProgramiv(id, GL_LINK_STATUS, &result);
	if (result == GL_FALSE) {
		glGetProgramiv(id, GL_INFO_LOG_LENGTH, &log_n);
		std::vector<char> error_msg(log_n);
		glGetProgramInfoLog(id, log_n, NULL, error_msg.data());
		m_assert(!log_n, std::string(std::begin(error_msg), std::end(error_msg)));
	}

	// clean up shaders
	glDetachShader(id, vertex_id);
	glDetachShader(id, frag_id);
	glDeleteShader(vertex_id);
	glDeleteShader(frag_id);
}

unsigned int GLShader::get_uniform_location(const std::string & var) {
	return glGetUniformLocation(id, var.c_str());
}

void GLShader::upload_uniform_matrix4fv(unsigned int var_id, float* start) {
	glUniformMatrix4fv(var_id, 1, GL_FALSE, start);
}

} // Namespace Raekor
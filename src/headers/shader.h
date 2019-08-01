#pragma once

#include "pch.h"
#include "util.h"

namespace Raekor {

class Shader {
public:
	static Shader* construct(std::string fp, std::string vertex);
	virtual const void bind() const = 0;
	virtual const void unbind() const = 0;

};

class GLShader : public Shader {
public:
	~GLShader() { glDeleteProgram(id); }
	GLShader(std::string frag, std::string vert);

	const void bind() const override { glUseProgram(id); }
	const void unbind() const override { glUseProgram(0); }

	unsigned int get_uniform_location(const std::string& var);
	void upload_uniform_matrix4fv(unsigned int var_id, float* start);

protected:
	unsigned int id;
};

} // Namespace Raekor

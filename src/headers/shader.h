#pragma once

namespace Raekor {

class GLShader {
public:
	~GLShader() { glDeleteProgram(id); }
	GLShader(std::string frag, std::string vert);

	inline unsigned int get_id() const { return id; }

	inline const void bind() const { glUseProgram(id); }
	inline const void unbind() const { glUseProgram(0); }

	unsigned int get_uniform_location(const std::string& var);
	void upload_uniform_matrix4fv(unsigned int var_id, float* start);

protected:
	unsigned int id;
};

} // Namespace Raekor

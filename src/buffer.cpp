#include "pch.h"
#include "buffer.h"
#include "renderer.h"
#include "util.h"

#ifdef _WIN32
	#include "DXBuffer.h"
#endif

namespace Raekor {

VertexBuffer* VertexBuffer::construct(const std::vector<glm::vec3>& vertices) {
	auto active = Renderer::get_activeAPI();
	switch(active) {
		case RenderAPI::OPENGL: {
			return new GLVertexBuffer(vertices);
		} break;
#ifdef _WIN32
		case RenderAPI::DIRECTX11: {
			return new DXVertexBuffer(vertices);
		} break;
#endif
	}
	return nullptr;
}

IndexBuffer* IndexBuffer::construct(const std::vector<unsigned int>& indices) {
	auto active = Renderer::get_activeAPI();
	switch (active) {
		case RenderAPI::OPENGL: {
			return new GLIndexBuffer(indices);
		} break;
#ifdef _WIN32
		case RenderAPI::DIRECTX11: {
			return new DXIndexBuffer(indices);
		} break;
#endif
	}
	return nullptr;
}

GLVertexBuffer::GLVertexBuffer(const std::vector<glm::vec3>& vertices) {
	id = gen_gl_buffer(vertices, GL_ARRAY_BUFFER);
}

void GLVertexBuffer::bind() const {
	//set attribute buffer for model vertices
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, id);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
}
void GLVertexBuffer::unbind() const {
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDisableVertexAttribArray(0);
}

GLIndexBuffer::GLIndexBuffer(const std::vector<unsigned int>& indices) {
	id = gen_gl_buffer(indices, GL_ELEMENT_ARRAY_BUFFER);
}

void GLIndexBuffer::bind() const {
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
}

void GLIndexBuffer::unbind() const {
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

} // namespace Raekor
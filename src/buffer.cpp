#include "pch.h"
#include "buffer.h"
#include "renderer.h"
#include "util.h"

#ifdef _WIN32
    #include "DXBuffer.h"
#endif

namespace Raekor {

VertexBuffer* VertexBuffer::construct(const std::vector<Vertex>& vertices) {
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

IndexBuffer* IndexBuffer::construct(const std::vector<Index>& indices) {
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

GLVertexBuffer::GLVertexBuffer(const std::vector<Vertex>& vertices) {
    id = gen_gl_buffer(vertices, GL_ARRAY_BUFFER);
}

void GLVertexBuffer::bind() const {
    // bind our vertex buffer and set its layout
    glBindBuffer(GL_ARRAY_BUFFER, id);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, 0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (const void*)(3 * sizeof(float)));
}
void GLVertexBuffer::unbind() const {
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

GLIndexBuffer::GLIndexBuffer(const std::vector<Index>& indices) {
    count = indices.size() * 3;
    id = gen_gl_buffer(indices, GL_ELEMENT_ARRAY_BUFFER);
}

void GLIndexBuffer::bind() const {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
}

void GLIndexBuffer::unbind() const {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

} // namespace Raekor
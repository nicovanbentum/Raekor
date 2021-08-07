#include "pch.h"
#include "buffer.h"
#include "renderer.h"
#include "util.h"

#ifdef _WIN32
    #include "platform/windows/DXBuffer.h"
    #include "platform/windows/DXResourceBuffer.h"
#endif

namespace Raekor {

ResourceBuffer* ResourceBuffer::construct(size_t size) {
    auto activeAPI = Renderer::getActiveAPI();
    switch (activeAPI) {
    case RenderAPI::OPENGL: {
        return nullptr;
    } break;
#ifdef _WIN32
    case RenderAPI::DIRECTX11: {
        return new DXResourceBuffer(size);
    } break;
#endif
    }
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

InputLayout::InputLayout(const std::initializer_list<Element> elementList) : layout(elementList), stride(0) {
    uint32_t offset = 0;
    for (auto& element : layout) {
        element.offset = offset;
        offset += element.size;
        stride += element.size;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

InputLayout::InputLayout(const std::vector<Element> elementList) : layout(elementList), stride(0) {
    uint32_t offset = 0;
    for (auto& element : layout) {
        element.offset = offset;
        offset += element.size;
        stride += element.size;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

VertexBuffer* VertexBuffer::construct(const std::vector<Vertex>& vertices) {
    auto active = Renderer::getActiveAPI();
    switch(active) {
        case RenderAPI::OPENGL: {
            return nullptr;
        } break;
#ifdef _WIN32
        case RenderAPI::DIRECTX11: {
            return new DXVertexBuffer(vertices);
        } break;
#endif
    }
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

IndexBuffer* IndexBuffer::construct(const std::vector<Triangle>& indices) {
    auto active = Renderer::getActiveAPI();
    switch (active) {
        case RenderAPI::OPENGL: {
            return nullptr;
        } break;
#ifdef _WIN32
        case RenderAPI::DIRECTX11: {
            return new DXIndexBuffer(indices);
        } break;
#endif
    }
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void glVertexBuffer::loadVertices(const Vertex* vertices, size_t count) {
    if (id) glDeleteBuffers(1, &id);
    glCreateBuffers(1, &id);
    glNamedBufferData(id, sizeof(Vertex) * count, vertices, GL_STATIC_DRAW);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void glVertexBuffer::loadVertices(float* vertices, size_t count) {
    glCreateBuffers(1, &id);
    glNamedBufferData(id, sizeof(float) * count, vertices, GL_STATIC_DRAW);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: rework this entire thing, consider VAO's
void glVertexBuffer::bind() const {
    glBindBuffer(GL_ARRAY_BUFFER, id);
    GLuint index = 0;
    for (const auto& element : inputLayout) {
        auto shaderType = static_cast<glShaderType>(element.type);
        glEnableVertexAttribArray(index);
        glVertexAttribPointer(
            index, // hlsl layout index
            shaderType.count, // number of types, e.g 3 floats
            shaderType.glType, // type, e.g float
            GL_FALSE, // normalized?
            (GLsizei)inputLayout.getStride(), // stride of the entire layout
            (const void*)((intptr_t)element.offset) // starting offset, casted up
        );
        index++;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void glVertexBuffer::setLayout(const InputLayout& layout) {
    inputLayout = layout;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void glVertexBuffer::destroy() {
    if (id) glDeleteBuffers(1, &id);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void glIndexBuffer::loadFaces(const Triangle* indices, size_t count) {
    glCreateBuffers(1, &id);
    glNamedBufferData(id, sizeof(Triangle) * count, indices, GL_STATIC_DRAW);
    this->count = static_cast<uint32_t>(count * 3);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void glIndexBuffer::loadIndices(uint32_t* indices, size_t count) {
    glCreateBuffers(1, &id);
    glNamedBufferData(id, sizeof(uint32_t) * count, indices, GL_STATIC_DRAW);
    this->count = static_cast<uint32_t>(count);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void glIndexBuffer::bind() const {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void glIndexBuffer::destroy() {
    if (id) glDeleteBuffers(1, &id);
}

glShaderType::glShaderType(ShaderType type) : glType(0), count(0) {
    switch (type) {
        case ShaderType::FLOAT1: {
            glType = GL_FLOAT;
            count = 1;
        } break;
        case ShaderType::FLOAT2: {
            glType = GL_FLOAT;
            count = 2;
        } break;
        case ShaderType::FLOAT3: {
            glType = GL_FLOAT;
            count = 3;
        } break;
        case ShaderType::FLOAT4: {
            glType = GL_FLOAT;
            count = 4;
        } break;
        case ShaderType::INT4: {
            glType = GL_INT;
            count = 4;
        } break;
    }
}

} // namespace Raekor
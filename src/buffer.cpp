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

    glUniformBuffer::glUniformBuffer() {
        glGenBuffers(1, &id);
    }

    glUniformBuffer::glUniformBuffer(size_t size) {
        glGenBuffers(1, &id);
        setSize(size);
    }

    void glUniformBuffer::setSize(size_t size) {
        glBindBuffer(GL_UNIFORM_BUFFER, id);
        glBufferData(GL_UNIFORM_BUFFER, size, NULL, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    void glUniformBuffer::update(void* data, const size_t size) const {
        glBindBuffer(GL_UNIFORM_BUFFER, id);
        void* dataPtr = glMapBuffer(GL_UNIFORM_BUFFER, GL_READ_WRITE);
        memcpy(dataPtr, data, size);
        glUnmapBuffer(GL_UNIFORM_BUFFER);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    void glUniformBuffer::bind(uint8_t slot) const {
        glBindBufferBase(GL_UNIFORM_BUFFER, slot, id);
    }

uint32_t size_of(ShaderType type) {
    switch (type) {
    case ShaderType::FLOAT1: return sizeof(float);
    case ShaderType::FLOAT2: return sizeof(float) * 2;
    case ShaderType::FLOAT3: return sizeof(float) * 3;
    case ShaderType::FLOAT4: return sizeof(float) * 4;
    default: return 0;
    }
}

glShaderType::glShaderType(ShaderType type) {
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
    }
}

InputLayout::InputLayout(const std::initializer_list<Element> elementList) : layout(elementList), stride(0) {
    uint32_t offset = 0;
    for (auto& element : layout) {
        element.offset = offset;
        offset += element.size;
        stride += element.size;
    }
}

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

void glVertexBuffer::loadVertices(Vertex* vertices, size_t count) {
    id = glCreateBuffer(vertices, count, GL_ARRAY_BUFFER);
}

void glVertexBuffer::bind() const {
    glBindBuffer(GL_ARRAY_BUFFER, id);
    GLuint index = 0;
    for (auto& element : inputLayout) {
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

void glVertexBuffer::setLayout(const InputLayout& layout) const {
    inputLayout = layout;
}

void glIndexBuffer::loadFaces(Triangle* indices, size_t count) {
    id = glCreateBuffer(indices, count, GL_ELEMENT_ARRAY_BUFFER);
    this->count = static_cast<uint32_t>(count * 3);
}

void glIndexBuffer::loadIndices(uint32_t* indices, size_t count) {
    id = glCreateBuffer(indices, count, GL_ELEMENT_ARRAY_BUFFER);
    this->count = static_cast<uint32_t>(count);
}


void glIndexBuffer::bind() const {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
}

} // namespace Raekor
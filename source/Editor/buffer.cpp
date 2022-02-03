#include "pch.h"
#include "buffer.h"
#include "renderer.h"
#include "Raekor/util.h"

namespace Raekor {

glShaderType::glShaderType(ShaderType type) : glType(0), count(0) {
    switch (type) {
        case ShaderType::FLOAT:
        case ShaderType::FLOAT2:
        case ShaderType::FLOAT3:
        case ShaderType::FLOAT4:
            glType = GL_FLOAT;
            count = size_of(type) / uint8_t(sizeof(float));
            break;
        case ShaderType::INT:
        case ShaderType::INT2:
        case ShaderType::INT3:
        case ShaderType::INT4:
            glType = GL_INT;
            count = size_of(type) / uint8_t(sizeof(int));
            break;
    }
}



glVertexLayout& glVertexLayout::attribute(const std::string& name, ShaderType type) {
    auto& attribute = layout.emplace_back(name, type);
    attribute.offset = stride;
    stride += attribute.size;
    return *this;
}



void glVertexLayout::bind() {
    GLuint index = 0;
    for (const auto& element : layout) {
        auto shaderType = static_cast<glShaderType>(element.type);
        glEnableVertexAttribArray(index);
        glVertexAttribPointer(
            index, // layout index
            shaderType.count, // number of types, e.g 3 floats
            shaderType.glType, // type, e.g float
            GL_FALSE, // normalized?
            (GLsizei)stride, // stride of the entire layout
            (const void*)((intptr_t)element.offset) // starting offset, casted up
        );
        index++;
    }
}

} // namespace Raekor
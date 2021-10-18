#pragma once

#include "util.h"
#include "shader.h"

namespace Raekor {

class ResourceBuffer {
public:
    virtual ~ResourceBuffer() {}
    static ResourceBuffer* construct(size_t size);

    // update and bind the resource buffer to one of 15 slots
    virtual void update(void* data, const size_t size) const = 0;
    virtual void bind(uint8_t slot) const = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

enum class ShaderType {
    FLOAT1, FLOAT2, FLOAT3, FLOAT4,
    INT4
};

//////////////////////////////////////////////////////////////////////////////////////////////////

constexpr uint32_t size_of(ShaderType type) {
    switch (type) {
    case ShaderType::FLOAT1:    return sizeof(float);
    case ShaderType::FLOAT2:    return sizeof(float) * 2;
    case ShaderType::FLOAT3:    return sizeof(float) * 3;
    case ShaderType::FLOAT4:    return sizeof(float) * 4;
    case ShaderType::INT4:      return sizeof(int) * 4;
    default: return 0;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

struct glShaderType {
    GLenum glType;
    uint8_t count;

    glShaderType(ShaderType shaderType);
};

//////////////////////////////////////////////////////////////////////////////////////////////////

struct Element {
    std::string name;
    ShaderType type;
    uint32_t size;
    uint32_t offset;

    Element(const std::string & pName, ShaderType type) : 
        name(pName), type(type), size(size_of(type)), offset(0) {}
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class InputLayout {
public:
    InputLayout() = default;
    InputLayout(const std::initializer_list<Element> elementList);
    InputLayout(const std::vector<Element> elementList);

    inline size_t size() const { return layout.size(); }
    inline uint64_t getStride() const { return stride; }

    auto begin() { return layout.begin(); }
    auto begin() const { return layout.begin(); }
    auto end() { return layout.end(); }
    auto end() const { return layout.end(); }

private:
    uint64_t stride;
    std::vector<Element> layout;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

struct Vertex {
    constexpr Vertex(glm::vec3 p = {}, glm::vec2 uv = {}, glm::vec3 n = {}, glm::vec3 t = {}, glm::vec3 b = {}) :
        pos(p), uv(uv), normal(n), tangent(t), binormal(b) {}

    glm::vec3 pos;
    glm::vec2 uv;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec3 binormal;

    static constexpr uint8_t attributeCount = 5;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

struct Triangle {
    constexpr Triangle(uint32_t _p1 = {}, uint32_t _p2 = {}, uint32_t _p3 = {}) :
        p1(_p1), p2(_p2), p3(_p3) {}

    uint32_t p1, p2, p3;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class VertexBuffer {
public:
    virtual ~VertexBuffer() {}
    static VertexBuffer* construct(const std::vector<Vertex>& vertices);
    virtual void bind() const = 0;
    virtual void setLayout(const InputLayout& layout) const = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class IndexBuffer {
public:
    virtual ~IndexBuffer() {}
    static IndexBuffer* construct(const std::vector<Triangle>& indices);
    virtual void bind() const = 0;
    inline const uint32_t getCount() const { return count; }

protected:
    uint32_t count;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class glVertexBuffer {
public:
    glVertexBuffer() = default;
    void loadVertices(const Vertex* vertices, size_t count);
    void loadVertices(const float* vertices, size_t count);
    void bind() const;
    void setLayout(const InputLayout& layout);
    uint64_t getStride() { return inputLayout.getStride(); }

    void destroy();

    unsigned int id = 0;
private:
    InputLayout inputLayout;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class glIndexBuffer {
public:
    glIndexBuffer() = default;
    void loadFaces(const Triangle* faces, size_t count);
    void loadIndices(uint32_t* indices, size_t count);
    void bind() const;

    void destroy();

    uint32_t count;

    unsigned int id = 0;
};


} // namespace Raekor
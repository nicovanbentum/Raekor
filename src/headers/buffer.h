#pragma once

#include "util.h"
#include "shader.h"
#include "renderer.h"

namespace Raekor {

class ResourceBuffer {
public:
    virtual ~ResourceBuffer() {}
    static ResourceBuffer* construct(size_t size);

    // update and bind the resource buffer to one of 15 slots
    virtual void update(void* data, const size_t size) const = 0;
    virtual void bind(uint8_t slot) const = 0;
};

class glUniformBuffer {
public:
    glUniformBuffer();
    ~glUniformBuffer();
    glUniformBuffer(size_t size);

    void setSize(size_t);
    void bind(uint8_t slot) const;
    void update(void* data, const size_t size);

private:
    void* dataPtr;
    uint32_t id;
};

enum class ShaderType {
    FLOAT1, FLOAT2, FLOAT3, FLOAT4,
    INT4
};

uint32_t size_of(ShaderType type);

struct glShaderType {
    GLenum glType;
    uint8_t count;

    glShaderType(ShaderType type);
};

struct Element {
    std::string name;
    ShaderType type;
    uint32_t size;
    uint32_t offset;

    Element(const std::string & pName, ShaderType type) : 
        name(pName), type(type), size(size_of(type)), offset(0) {}
};

class InputLayout {
public:
    InputLayout(const std::initializer_list<Element> elementList);
    InputLayout() {}

    inline size_t size() const { return layout.size(); }
    inline uint64_t getStride() const { return stride; }

    std::vector<Element>::iterator begin() { return layout.begin(); }
    std::vector<Element>::iterator end() { return layout.end(); }
    std::vector<Element>::const_iterator begin() const { return layout.begin(); }
    std::vector<Element>::const_iterator end() const { return layout.end(); }

private:
    std::vector<Element> layout;
    uint64_t stride;
};

struct Vertex {
    alignas(16) glm::vec3 pos;
    alignas(16) glm::vec2 uv;
    alignas(16) glm::vec3 normal;
    alignas(16) glm::vec3 tangent;
    alignas(16) glm::vec3 binormal;

    static constexpr uint8_t attributeCount = 5;
};

struct Triangle {
    Triangle() {}
    Triangle(uint32_t _p1, uint32_t _p2, uint32_t _p3) : p1(_p1), p2(_p2), p3(_p3) {}

    uint32_t p1, p2, p3;
};

class VertexBuffer {
public:
    virtual ~VertexBuffer() {}
    static VertexBuffer* construct(const std::vector<Vertex>& vertices);
    virtual void bind() const = 0;
    virtual void setLayout(const InputLayout& layout) const = 0;
};

class IndexBuffer {
public:
    virtual ~IndexBuffer() {}
    static IndexBuffer* construct(const std::vector<Triangle>& indices);
    virtual void bind() const = 0;
    inline const uint32_t getCount() const { return count; }

protected:
    uint32_t count;
};

class glVertexBuffer {
public:
    glVertexBuffer() {}
    void loadVertices(Vertex* vertices, size_t count);
    void bind() const;
    void setLayout(const InputLayout& layout) const;

    unsigned int id = 0;
private:
    mutable InputLayout inputLayout;
};

class glIndexBuffer {
public:
    glIndexBuffer() {}
    void loadFaces(Triangle* faces, size_t count);
    void loadIndices(uint32_t* indices, size_t count);
    void bind() const ;

    uint32_t count;

private:
    unsigned int id = 0;
};

template<typename Type>
uint32_t glCreateBuffer(Type* data, size_t count, GLenum target) {
    unsigned int id;
    glCreateBuffers(1, &id);
    glNamedBufferData(id, count * sizeof(Type), data, GL_STATIC_DRAW);
    glBindBuffer(target, 0);
    return id;
}

} // namespace Raekor
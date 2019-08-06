#pragma once

namespace Raekor {

// directx has it's own mappings for floats an structures like R32G32B32_FLOAT for a 3 floats. 
// openGL wants seperate components, like count = 3, type = float

enum class ShaderType {
    FLOAT1, FLOAT2, FLOAT3, FLOAT4
};

uint32_t size_of(ShaderType type);

struct GLShaderType {
    GLenum type;
    uint8_t count;

    GLShaderType(ShaderType shader_type);
};

struct Element {
    std::string name;
    ShaderType type;
    uint32_t size;
    uint32_t offset;

    Element(const std::string& p_name, ShaderType type) : 
        name(p_name), type(type), size(size_of(type)), offset(0) {}
};

class InputLayout {
public:
    InputLayout(const std::initializer_list<Element> element_list);

    inline uint64_t get_stride() const { return stride; }

    std::vector<Element>::iterator begin() { return layout.begin(); }
    std::vector<Element>::iterator end() { return layout.end(); }
    std::vector<Element>::const_iterator begin() const { return layout.begin(); }
    std::vector<Element>::const_iterator end() const { return layout.end(); }

private:
    std::vector<Element> layout;
    uint64_t stride;
};


struct Vertex {
    glm::vec3 pos;
    glm::vec2 uv;

    // operators needed for indexing algorithm
    bool operator<(const Raekor::Vertex & rhs) const {
        return memcmp((void*)this, (void*)&rhs, sizeof(Raekor::Vertex)) > 0;
    };
    bool operator>(const Raekor::Vertex & rhs) const {
        return memcmp((void*)this, (void*)&rhs, sizeof(Raekor::Vertex)) < 0;
    };
};

struct Index {
    uint32_t f1, f2, f3;
};

class VertexBuffer {
public:
    virtual ~VertexBuffer() {}
    static VertexBuffer* construct(const std::vector<Vertex>& vertices);
    virtual void bind() const = 0;
    virtual void unbind() const = 0;
};

class IndexBuffer {
public:
    virtual ~IndexBuffer() {}
    static IndexBuffer* construct(const std::vector<Index>& indices);
    virtual void bind() const = 0;
    virtual void unbind() const = 0;
    inline unsigned int get_count() const { return count; }

protected:
    unsigned int count;
};

class GLVertexBuffer : public VertexBuffer {
public:
    GLVertexBuffer(const std::vector<Vertex>& vertices);
    virtual void bind() const override;
    virtual void unbind() const override;

private:
    unsigned int id = 0;
};

class GLIndexBuffer : public IndexBuffer {
public:
    GLIndexBuffer(const std::vector<Index>& indices);
    virtual void bind() const override;
    virtual void unbind() const override;

private:
    unsigned int id = 0;
};

} // namespace Raekor
#pragma once

namespace Raekor {

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
    inline unsigned int get_id() const { return id; }

private:
    unsigned int id = 0;
};

class GLIndexBuffer : public IndexBuffer {
public:
    GLIndexBuffer(const std::vector<Index>& indices);
    virtual void bind() const override;
    virtual void unbind() const override;
    inline unsigned int get_id() const { return id; }

private:
    unsigned int id = 0;
};

} // namespace Raekor
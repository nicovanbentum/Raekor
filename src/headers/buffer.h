#pragma once

namespace Raekor {

class VertexBuffer {
public:
    virtual ~VertexBuffer() {}
    VertexBuffer* construct(const std::vector<glm::vec3>& vertices);
    virtual void bind() const = 0;
    virtual void unbind() const = 0;
};

class IndexBuffer {
public:
    virtual ~IndexBuffer() {}
    IndexBuffer* construct(const std::vector<unsigned int>& indices);
    virtual void bind() const = 0;
    virtual void unbind() const = 0;
};

class GLVertexBuffer : public VertexBuffer {
public:
    GLVertexBuffer(const std::vector<glm::vec3>& vertices);
    virtual void bind() const override;
    virtual void unbind() const override;

private:
    unsigned int id = 0;
};

class GLIndexBuffer : public IndexBuffer {
public:
    GLIndexBuffer(const std::vector<unsigned int>& indices);
    virtual void bind() const override;
    virtual void unbind() const override;

private:
    unsigned int id = 0;
    unsigned int size = 0;
};

} // namespace Raekor
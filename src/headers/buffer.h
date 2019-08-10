#pragma once

#include "util.h"
#include "shader.h"

namespace Raekor {

struct cb_vs {
    glm::mat4 MVP;
};

template<typename T>
struct ShaderBuffer {
    std::string handle;
    T structure;

    ShaderBuffer(const std::string& handle) : handle(handle) {}
    ShaderBuffer() {}
};

class ResourceBuffer {
public:
    virtual ~ResourceBuffer() {}
    virtual void bind() const = 0;
};

template<typename T>
class GLResourceBuffer : public ResourceBuffer {
public:
    GLResourceBuffer(Raekor::Shader* shader, const ShaderBuffer<T>& shader_buffer) : data(shader_buffer) {
        GLShader* gl_shader = dynamic_cast<GLShader*>(shader);
        program_id = gl_shader->get_id();
        handle = glGetUniformBlockIndex(program_id, shader_buffer.handle.c_str());

        glGenBuffers(1, &id);
        glBindBuffer(GL_UNIFORM_BUFFER, id);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(T), NULL, GL_DYNAMIC_DRAW);
        void* data_ptr = glMapBuffer(GL_UNIFORM_BUFFER, GL_READ_WRITE);
        m_assert(data_ptr, "failed to map memory");
        glUnmapBuffer(GL_UNIFORM_BUFFER);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    virtual void bind() const override {
        // update the resource data
        glBindBuffer(GL_UNIFORM_BUFFER, id);
        void* data_ptr = glMapBuffer(GL_UNIFORM_BUFFER, GL_READ_WRITE);
        m_assert(data_ptr, "failed to map memory");
        memcpy(data_ptr, &data.structure, sizeof(T));
        glUnmapBuffer(GL_UNIFORM_BUFFER);

        // bind the buffer to a slot
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, id);
        glUniformBlockBinding(program_id, handle, 0);
    }

    T& get_data() {
        return data.structure;
    }

    ShaderBuffer<T> data;
    unsigned int id;
    unsigned int handle;
    unsigned int program_id;
};

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

struct ShaderResource {
    const void* data;
    ShaderType type;
};

class InputLayout {
public:
    InputLayout(const std::initializer_list<Element> element_list);

    inline size_t size() const { return layout.size(); }
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
    virtual void set_layout(const InputLayout& layout) const = 0;
};

class IndexBuffer {
public:
    virtual ~IndexBuffer() {}
    static IndexBuffer* construct(const std::vector<Index>& indices);
    virtual void bind() const = 0;
    inline unsigned int get_count() const { return count; }

protected:
    unsigned int count;
};

class GLVertexBuffer : public VertexBuffer {
public:
    GLVertexBuffer(const std::vector<Vertex>& vertices);
    virtual void bind() const override;
    virtual void set_layout(const InputLayout& layout) const override;

private:
    unsigned int id = 0;
};

class GLIndexBuffer : public IndexBuffer {
public:
    GLIndexBuffer(const std::vector<Index>& indices);
    virtual void bind() const override;

private:
    unsigned int id = 0;
};

} // namespace Raekor
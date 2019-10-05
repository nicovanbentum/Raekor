#pragma once

#include "util.h"
#include "shader.h"
#include "renderer.h"

namespace Raekor {

// struct we send to the shaders
// TODO: figure out a common place for these
struct cb_vs {
    glm::mat4 MVP;
};

// forward declarations necessary cause of templates
template<typename T>
class GLResourceBuffer;

template<typename T>
class DXResourceBuffer;

// resource buffer class that takes a struct as template parameter
// the struct must match the one in shaders
template<typename T>
class ResourceBuffer {
public:
    virtual ~ResourceBuffer() {}

    static ResourceBuffer* construct() {
        auto active_api = Renderer::get_activeAPI();
        switch (active_api) {
            case RenderAPI::OPENGL: {
                return new GLResourceBuffer<T>;
                } break;
#ifdef _WIN32
            case RenderAPI::DIRECTX11: {
                return new DXResourceBuffer<T>;
            } break;
#endif
        }
        return nullptr;
    }
    // update and bind the resource buffer to one of 15 slots
    virtual void bind(uint8_t slot) const = 0;
    
    // function to  modify the data we send to the GPU
    T& get_data() {
        return data;
    }
protected:
    T data;
};

template<typename T>
class GLResourceBuffer : public ResourceBuffer<T> {
public:
    GLResourceBuffer() {
        glGenBuffers(1, &id);
        glBindBuffer(GL_UNIFORM_BUFFER, id);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(T), NULL, GL_DYNAMIC_DRAW);
        void* data_ptr = glMapBuffer(GL_UNIFORM_BUFFER, GL_READ_WRITE);
        m_assert(data_ptr, "failed to map memory");
        glUnmapBuffer(GL_UNIFORM_BUFFER);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    virtual void bind(uint8_t slot) const override {
        // update the resource data
        // bind the buffer
        glBindBuffer(GL_UNIFORM_BUFFER, id);
        // retrieve a pointer to the cpu memory that OpenGL reads from
        void* data_ptr = glMapBuffer(GL_UNIFORM_BUFFER, GL_READ_WRITE);
        m_assert(data_ptr, "failed to map memory");
        // copy the memory of our struct into the mapped memory
        memcpy(data_ptr, &this->data, sizeof(T));
        glUnmapBuffer(GL_UNIFORM_BUFFER);
        // bind the buffer to a slot
        glBindBufferBase(GL_UNIFORM_BUFFER, slot, id);
    }

private:
    unsigned int id;
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
    glm::vec3 normal;

    static constexpr uint8_t attribute_count = 3;
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
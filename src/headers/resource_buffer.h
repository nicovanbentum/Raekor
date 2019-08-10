#pragma once

#include "pch.h"
#include "util.h"
#include "DXRenderer.h"

namespace Raekor {

    struct cb_vs {
        glm::mat4 MVP;
    };

    template<typename T>
    struct ShaderBuffer {
        std::string handle;
        T structure;

        ShaderBuffer(const std::string& handle) : handle(handle) {}
    };

    template<typename T>
    class ResourceBuffer {
    public:
        virtual ~ResourceBuffer() {}

        ResourceBuffer<T>* construct(Raekor::Shader* shader) {
            auto active_api = Renderer::get_activeAPI();
            switch (active_api) {
            case RenderAPI::OPENGL: {
                return new typename Raekor::GLResourceBuffer<T>(shader);
            } break;
            case RenderAPI::DIRECTX11: {
                return new typename Raekor::DXResourceBuffer<T>(shader);
            } break;
            default: return nullptr;
            }
        }

        virtual void bind(uint8_t slot) const = 0;
    };


    template<typename T>
    class DXResourceBuffer : public ResourceBuffer<T> {
    public:
        DXResourceBuffer(Raekor::Shader* shader, const ShaderBuffer<T>& shader_buffer) {
            // describe the constant buffer, align the bytewidth to 16bit
            // TODO: Figure out if there's a better way to align, this seems like a hack
            D3D11_BUFFER_DESC cbdesc;
            cbdesc.Usage = D3D11_USAGE_DYNAMIC;
            cbdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            cbdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            cbdesc.MiscFlags = 0;
            cbdesc.ByteWidth = static_cast<UINT>(sizeof(T) + (16 - (sizeof(T) % 16)));
            cbdesc.StructureByteStride = 0;

            D3D11_SUBRESOURCE_DATA cbdata;
            cbdata.pSysMem = &data;
            cbdata.SysMemPitch = 0;
            cbdata.SysMemSlicePitch = 0;
            auto hr = D3D.device->CreateBuffer(&cbdesc, &cbdata, buffer.GetAddressOf());
            m_assert(SUCCEEDED(hr), "failed to create dx constant buffer");
        }

        void bind(uint8_t slot) const override {
            // update the buffer's data on the GPU
            D3D11_MAPPED_SUBRESOURCE resource;
            D3D.context->Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
            memcpy(resource.pData, &data, sizeof(T));
            D3D.context->Unmap(buffer.Get(), 0);

            // bind the buffer to its slot
            D3D.context->VSSetConstantBuffers(0, 1, buffer.GetAddressOf());
        }

        T& get_data() {
            return data.structure;
        }

    private:
        ShaderBuffer<T> data;
        com_ptr<ID3D11Buffer> buffer;
    };


    template<typename T>
    class GLResourceBuffer : public ResourceBuffer<T> {
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

        virtual void bind(uint8_t slot) const override {
            // update the resource data
            glBindBuffer(GL_UNIFORM_BUFFER, id);
            void* data_ptr = glMapBuffer(GL_UNIFORM_BUFFER, GL_READ_WRITE);
            m_assert(data_ptr, "failed to map memory");
            memcpy(data_ptr, &data.structure, sizeof(T));
            glUnmapBuffer(GL_UNIFORM_BUFFER);

            // bind the buffer to slot 0
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

} // namespace Raekor

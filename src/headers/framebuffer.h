#pragma once

#include "pch.h"

namespace Raekor {


    class FrameBuffer {
    public:
        struct ConstructInfo {
            glm::vec2 size;
            // opt
            bool depthOnly = false;
            bool writeOnly = false;
            bool HDR = false;
        };

    public:
        virtual ~FrameBuffer() {}
        static FrameBuffer* construct(ConstructInfo* info);
        virtual void bind() const = 0;
        virtual void unbind() const = 0;
        virtual void ImGui_Image() const = 0;
        virtual void resize(const glm::vec2& size) = 0;
        inline glm::vec2 get_size() const { return size; }

    protected:
        glm::vec2 size;
        unsigned int fbo_id;
        unsigned int rbo_id;
        unsigned int render_texture_id;
    };

    class GLFrameBuffer : public FrameBuffer {
    public:
        GLFrameBuffer(FrameBuffer::ConstructInfo* info);
        ~GLFrameBuffer();
        virtual void bind() const override;
        virtual void unbind() const override;
        virtual void bindTexture(uint32_t slot) const;
        virtual void ImGui_Image() const override;
        virtual void resize(const glm::vec2& size) override;

    private:
        GLenum format;
    };

} // Raekor

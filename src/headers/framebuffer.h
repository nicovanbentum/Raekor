#pragma once

#include "pch.h"
#include "texture.h"    

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
        virtual void ImGui_Image(glm::vec2 imgSize) const;
        virtual void resize(const glm::vec2& size) override;

    private:
        GLenum format;
    };


class glRenderbuffer {
public:
    friend class glFramebuffer;

    glRenderbuffer() {
        glGenRenderbuffers(1, &mID);
    }

    ~glRenderbuffer() {
        glDeleteRenderbuffers(1, &mID);
    }

    void init(uint32_t width, uint32_t height, GLenum format) {
        glBindRenderbuffer(GL_RENDERBUFFER, mID);
        glRenderbufferStorage(GL_RENDERBUFFER, format, width, height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0); 
    }

private:
    unsigned int mID;
};

class glFramebuffer {
public:
    glFramebuffer() {
        glGenFramebuffers(1, &mID);
    }

    ~glFramebuffer() {
        glDeleteFramebuffers(1, &mID);
    }

    void bind() {
        glBindFramebuffer(GL_FRAMEBUFFER, mID);
    }

    void unbind() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void attach(glTexture& texture, GLenum type) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, type, texture.mTarget, texture.mID, 0);
        
        // if its a color attachment and we haven't seen it before, store it
        for (int slot = GL_COLOR_ATTACHMENT0; slot < GL_COLOR_ATTACHMENT0 + GL_MAX_COLOR_ATTACHMENTS; slot++) {
            if (type == slot) {
                if (auto it = std::find(colorAttachments.begin(), colorAttachments.end(), type); it == colorAttachments.end()) {
                    std::cout << "storing new color attachment" << std::endl;
                    colorAttachments.push_back(type);
                    break;
                }
                std::cout << "reusing old color attachment" << std::endl;
            }
        }
        
        // tell it to use all stored color attachments so far for rendering
        if (!colorAttachments.empty()) {
            glNamedFramebufferDrawBuffers(mID, static_cast<GLsizei>(colorAttachments.size()), colorAttachments.data());
        // else we assume its a depth only attachment for now
        } else {
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        }
    }

    void attach(glRenderbuffer& buffer, GLenum attachment) {
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, buffer.mID);
    }

private:
    std::vector<unsigned int> colorAttachments;
    unsigned int mID;
};

} // Raekor

#pragma once

#include "pch.h"
#include "texture.h"    

namespace Raekor {

/*
    old framebuffer api, should be removed
*/
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
    virtual void Image() const = 0;
    virtual void resize(const glm::vec2& size) = 0;
    inline glm::vec2 getSize() const { return size; }

protected:
    glm::vec2 size;
    unsigned int fboID;
    unsigned int rboID;
    unsigned int textureID;
};


class glRenderbuffer {
public:
    friend class glFramebuffer;
    
    glRenderbuffer();
    ~glRenderbuffer();
    void init(uint32_t width, uint32_t height, GLenum format);

private:
    unsigned int mID;
};

class glFramebuffer {
public:
    glFramebuffer();
    ~glFramebuffer();

    void bind();
    void unbind();
    void attach(glTexture2D& texture, GLenum type);
    void attach(glRenderbuffer& buffer, GLenum attachment);
    void attach(glTextureCube& texture, GLenum type, uint8_t face) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, type,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, texture.mID, 0);
    }

private:
    std::vector<unsigned int> colorAttachments;
    unsigned int mID;
};

} // Raekor

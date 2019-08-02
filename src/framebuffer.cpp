#include "pch.h"
#include "util.h"
#include "framebuffer.h"

namespace Raekor {

FrameBuffer* FrameBuffer::construct(const glm::vec2& new_size) {
    return new GLFrameBuffer(new_size);
}

GLFrameBuffer::GLFrameBuffer(const glm::vec2& new_size) {
    size = new_size;
    // gen and bind our frame buffer
    glGenFramebuffers(1, &fbo_id);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);

    // gen and bind our render texture
    glGenTextures(1, &render_texture_id);
    glBindTexture(GL_TEXTURE_2D, render_texture_id);

    // create the 2d texture image
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (GLsizei)size.x, (GLsizei)size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // unbind the texture and attach it to the framebuffer
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render_texture_id, 0);

    // gen and bind render buffer
    glGenRenderbuffers(1, &rbo_id);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo_id);

    // create a buffer storage that describes the render buffer, unbind after
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1280, 720);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // attach the render buffer to the frame buffer
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo_id);

    // check if succesful
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        m_assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "failed to create frame buffer");
    }

    // unbind the frame buffer for now
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLFrameBuffer::~GLFrameBuffer() {
    glDeleteTextures(1, &render_texture_id);
    glDeleteFramebuffers(1, &fbo_id);
    glDeleteRenderbuffers(1, &rbo_id);
}

void GLFrameBuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
}

void GLFrameBuffer::unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLFrameBuffer::resize(const glm::vec2& new_size) {
    size = new_size;
    // bind our texture and resets its attributes
    glBindTexture(GL_TEXTURE_2D, render_texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (GLsizei)new_size.x, (GLsizei)new_size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

    // unbind the texture and attach it to the framebuffer
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render_texture_id, 0);
}

} // Raekor

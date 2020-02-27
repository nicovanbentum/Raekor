#include "pch.h"
#include "util.h"
#include "framebuffer.h"
#include "renderer.h"

#ifdef _WIN32
#include "DXFrameBuffer.h"
#endif

namespace Raekor {

FrameBuffer* FrameBuffer::construct(FrameBuffer::ConstructInfo* info) {
    auto active_api = Renderer::get_activeAPI();
    switch (active_api) {
        case RenderAPI::OPENGL: {
            return new GLFrameBuffer(info);
        } break;
#ifdef _WIN32
        case RenderAPI::DIRECTX11: {
            return new DXFrameBuffer(info);
        } break;
#endif
    }
    return nullptr;
}

GLFrameBuffer::GLFrameBuffer(FrameBuffer::ConstructInfo* info) {
    size = info->size;
    // gen and bind our frame buffer
    glGenFramebuffers(1, &fbo_id);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);

    // gen and bind our render texture
    glGenTextures(1, &render_texture_id);
    glBindTexture(GL_TEXTURE_2D, render_texture_id);

    format = info->HDR ? GL_RGB16F : GL_RGB;

    // create the 2d texture image
    if (info->depthOnly) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, (GLsizei)size.x, (GLsizei)size.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, format, (GLsizei)size.x, (GLsizei)size.y, 0, GL_RGB, GL_FLOAT, NULL);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // unbind the texture and attach it to the framebuffer
    glBindTexture(GL_TEXTURE_2D, 0);

    GLenum attachment = info->depthOnly ? GL_DEPTH_COMPONENT : GL_COLOR_ATTACHMENT0;
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, render_texture_id, 0);

    // gen and bind render buffer
    glGenRenderbuffers(1, &rbo_id);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo_id);

    // create a buffer storage that describes the render buffer, unbind after
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, (GLsizei)size.x, (GLsizei)size.y);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // attach the render buffer to the frame buffer
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo_id);

    // check if succesful
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        m_assert((glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE), "failed to create frame buffer");
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
    glViewport(0, 0, (GLsizei)size.x, (GLsizei)size.y);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
}

void GLFrameBuffer::unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLFrameBuffer::bindTexture(uint32_t slot) const {
    glBindTextureUnit(slot, render_texture_id);
}

void GLFrameBuffer::ImGui_Image() const {
    ImGui::Image((void*)((intptr_t)render_texture_id), ImVec2(size.x, size.y), { 0,1 }, { 1,0 });
}

void GLFrameBuffer::ImGui_Image(glm::vec2 imgSize) const {
    ImGui::Image((void*)((intptr_t)render_texture_id), ImVec2(imgSize.x, imgSize.y), { 0,1 }, { 1,0 });
}

void GLFrameBuffer::resize(const glm::vec2& new_size) {
    if (size == new_size) return;
    size = new_size;
    // bind the render texture and reset its attributes
    glBindTexture(GL_TEXTURE_2D, render_texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, format, (GLsizei)new_size.x, (GLsizei)new_size.y, 0, GL_RGB, GL_FLOAT, NULL);


    // unbind the texture and (re-)attach it to the framebuffer
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render_texture_id, 0);

    glBindRenderbuffer(GL_RENDERBUFFER, rbo_id);

    // create a buffer storage that describes the render buffer, unbind after
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, (GLsizei)size.x, (GLsizei)size.y);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // attach the render buffer to the frame buffer
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo_id);
}

} // Raekor

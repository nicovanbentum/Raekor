#include "pch.h"
#include "util.h"
#include "framebuffer.h"
#include "renderer.h"

#ifdef _WIN32
#include "platform/windows/DXFrameBuffer.h"
#endif

namespace Raekor {

FrameBuffer* FrameBuffer::construct(FrameBuffer::ConstructInfo* info) {
    auto activeAPI = Renderer::getActiveAPI();
    switch (activeAPI) {
        case RenderAPI::OPENGL: {
            return nullptr;
        } break;
#ifdef _WIN32
        case RenderAPI::DIRECTX11: {
            return new DXFrameBuffer(info);
        } break;
#endif
    }
    return nullptr;
}

glRenderbuffer::glRenderbuffer() {
    glGenRenderbuffers(1, &mID);
}

glRenderbuffer::~glRenderbuffer() {
    glDeleteRenderbuffers(1, &mID);
}

void glRenderbuffer::init(uint32_t width, uint32_t height, GLenum format) {
    glBindRenderbuffer(GL_RENDERBUFFER, mID);
    glRenderbufferStorage(GL_RENDERBUFFER, format, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

glFramebuffer::glFramebuffer() {
    glCreateFramebuffers(1, &mID);
}

glFramebuffer::~glFramebuffer() {
    glDeleteFramebuffers(1, &mID);
}

void glFramebuffer::bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, mID);
}

void glFramebuffer::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void glFramebuffer::attach(glTexture2D& texture, GLenum type) {
    glNamedFramebufferTexture(mID, type, texture.mID, 0);

    // if its a color attachment and we haven't seen it before, store it
    if (type != GL_DEPTH_ATTACHMENT && type != GL_STENCIL_ATTACHMENT && type != GL_DEPTH_STENCIL_ATTACHMENT) {
        if (auto it = std::find(colorAttachments.begin(), colorAttachments.end(), type); it == colorAttachments.end()) {
            colorAttachments.push_back(type);
        }
    }

    // tell it to use all stored color attachments so far for rendering
    if (!colorAttachments.empty()) {
        glNamedFramebufferDrawBuffers(mID, static_cast<GLsizei>(colorAttachments.size()), colorAttachments.data());
        // else we assume its a depth only attachment for now
    }
    else {
        glNamedFramebufferDrawBuffer(mID, GL_NONE);
        glNamedFramebufferReadBuffer(mID, GL_NONE);
    }
}

void glFramebuffer::attach(glRenderbuffer& buffer, GLenum attachment) {
    glNamedFramebufferRenderbuffer(mID, attachment, GL_RENDERBUFFER, buffer.mID);
}

void glFramebuffer::attach(glTextureCube& texture, GLenum type, uint8_t face) {
    glNamedFramebufferTexture(mID, type, texture.mID, face);
}

} // Raekor

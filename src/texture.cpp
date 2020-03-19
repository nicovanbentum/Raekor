#include "pch.h"
#include "util.h"
#include "texture.h"
#include "renderer.h"

#ifdef _WIN32
#include "DXTexture.h"
#endif

namespace Raekor {

//////////////////////////////////////////////////////////////////////////////////

glTexture::glTexture(GLenum pTarget) {
    mTarget = pTarget;
    glGenTextures(1, &mID);
}

//////////////////////////////////////////////////////////////////////////////////

glTexture::~glTexture() {
    glDeleteTextures(1, &mID);
}

//////////////////////////////////////////////////////////////////////////////////

void glTexture::bind() {
    glBindTexture(mTarget, mID);
}

//////////////////////////////////////////////////////////////////////////////////

void glTexture::unbind() {
    glBindTexture(mTarget, 0);
}

//////////////////////////////////////////////////////////////////////////////////

void glTexture::bindToSlot(uint8_t slot) {
    glBindTextureUnit(slot, mID);
}

//////////////////////////////////////////////////////////////////////////////////

void glTexture::genMipMaps() {
    glGenerateMipmap(GL_TEXTURE_2D);
}

//////////////////////////////////////////////////////////////////////////////////

void glTexture::setFilter(Sampling::Filter filter) {
    int minFilter = 0, magFilter = 0;

    switch (filter) {
    case Sampling::Filter::None: {
        minFilter = magFilter = GL_NEAREST;
    } break;
    case Sampling::Filter::Bilinear: {
        minFilter = magFilter = GL_LINEAR;
    } break;
    case Sampling::Filter::Trilinear: {
        minFilter = GL_LINEAR_MIPMAP_LINEAR;
        magFilter = GL_LINEAR;
    } break;
    }

    glTexParameteri(mTarget, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(mTarget, GL_TEXTURE_MAG_FILTER, magFilter);
}

//////////////////////////////////////////////////////////////////////////////////

void glTexture::setWrap(Sampling::Wrap mode) {
    int wrapMode = 0;

    switch (mode) {
    case Sampling::Wrap::Repeat: {
        wrapMode = GL_REPEAT;
    } break;
    case Sampling::Wrap::ClampBorder: {
        wrapMode = GL_CLAMP_TO_BORDER;
    } break;
    case Sampling::Wrap::ClampEdge: {
        wrapMode = GL_CLAMP_TO_EDGE;
    } break;
    }

    glTexParameteri(mTarget, GL_TEXTURE_WRAP_S, wrapMode);
    glTexParameteri(mTarget, GL_TEXTURE_WRAP_T, wrapMode);
    glTexParameteri(mTarget, GL_TEXTURE_WRAP_R, wrapMode);
}

//////////////////////////////////////////////////////////////////////////////////

ImTextureID glTexture::ImGuiID() { return (void*)((intptr_t)mID); }

//////////////////////////////////////////////////////////////////////////////////

glTexture2D::glTexture2D() : glTexture(GL_TEXTURE_2D) {}

//////////////////////////////////////////////////////////////////////////////////

void glTexture2D::init(uint32_t width, uint32_t height, const Format::Format& format, const void* data) {
    glTexImage2D(mTarget, 0, format.intFormat, width, height, 0, format.extFormat, format.type, data);
}

//////////////////////////////////////////////////////////////////////////////////

glTextureCube::glTextureCube() : glTexture(GL_TEXTURE_CUBE_MAP) {}

//////////////////////////////////////////////////////////////////////////////////

void glTextureCube::init(uint32_t width, uint32_t height, uint8_t face, const Format::Format& format, const void* data) {
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, format.intFormat, width, height, 0, format.extFormat, format.type, data);
}

} // Namespace Raekor
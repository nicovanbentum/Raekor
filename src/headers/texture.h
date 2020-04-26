#pragma once

#include "pch.h"

namespace Raekor {

//////////////////////////////////////////////////////////////////////////////////

namespace Sampling {
    enum class Filter {
        None, Bilinear, Trilinear
    };

    enum class Wrap {
        Repeat, ClampEdge, ClampBorder
    };
}

//////////////////////////////////////////////////////////////////////////////////

namespace Format {
    struct Format {
        GLenum intFormat;
        GLenum extFormat;
        GLenum type;
    };

    static constexpr Format DEPTH           { GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT };
    static constexpr Format SRGBA_U8        { GL_SRGB_ALPHA, GL_RGBA, GL_UNSIGNED_BYTE };
    static constexpr Format RGBA_U8         { GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE };
    static constexpr Format RGBA_F16        { GL_RGBA16F, GL_RGBA, GL_FLOAT };
    static constexpr Format RGB_F16         { GL_RGB16F, GL_RGB, GL_FLOAT };
    static constexpr Format RGB_F           { GL_RGB, GL_RGB, GL_FLOAT };
    static constexpr Format RGBA_F          { GL_RGBA, GL_RGBA, GL_FLOAT };
}

//////////////////////////////////////////////////////////////////////////////////

class glTexture {
public:
    friend class glFramebuffer;

    glTexture(GLenum pTarget);
    ~glTexture();

    void bind();
    void unbind();
    void bindToSlot(uint8_t slot);

    void genMipMaps();
    void setFilter(Sampling::Filter filter);
    void setWrap(Sampling::Wrap mode);

    ImTextureID ImGuiID();

    void clear(const glm::vec3& colour);

protected:
    GLenum mTarget;
    unsigned int mID;
};

//////////////////////////////////////////////////////////////////////////////////

class glTexture2D : public glTexture {
public:
    glTexture2D();
    void init(uint32_t width, uint32_t height, const Format::Format& format, const void* data = nullptr);
};

//////////////////////////////////////////////////////////////////////////////////

class glTextureCube : public glTexture {
public:
    glTextureCube();
    void init(uint32_t width, uint32_t height, uint8_t face, const Format::Format& format, const void* data = nullptr);
};

class glTexture3D : public glTexture {

public:
    glTexture3D();
    void init(uint32_t width, uint32_t height, uint32_t depth, const Format::Format& format, const void* data);
    void bindForStorage(uint32_t slot, GLenum access, GLenum format) {
        bindToSlot(slot);
        glBindImageTexture(slot, mID, 0, GL_TRUE, 0, access, format);
    }
};



} //Namespace Raekor
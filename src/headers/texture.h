#pragma once

#include "pch.h"

namespace Raekor {

class Texture {
public:
    virtual ~Texture() {}
    static Texture* construct(const std::string& path);
    static Texture* construct(const std::array<std::string, 6>& face_files);
    static Texture* construct(const Stb::Image& image);
    virtual void bind(uint32_t slot) const = 0;
    std::string get_path() const { return filepath; }

protected:
    std::string filepath;

public:
    bool hasAlpha = false;
};

class GLTextureCube : public Texture {

public:
    GLTextureCube(const std::array<std::string, 6>& face_files);
    ~GLTextureCube();
    virtual void bind(uint32_t slot) const override;

private:
    unsigned int id;
};

namespace Sampling {
    enum class Filter {
        None, Bilinear, Trilinear
    };

    enum class Wrap {
        Repeat, ClampEdge, ClampBorder
    };
}

namespace Format {
    struct Format {
        GLenum intFormat;
        GLenum extFormat;
        GLenum type;
    };

    static constexpr Format Depth   { GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT };
    static constexpr Format sRGBA   { GL_SRGB_ALPHA, GL_RGBA, GL_UNSIGNED_BYTE };
    static constexpr Format RGBA    { GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE };
    static constexpr Format HDR     { GL_RGBA16F, GL_RGBA, GL_FLOAT };
    static constexpr Format SDR     { GL_RGB, GL_RGB, GL_FLOAT };
}

class glTexture {
public:
    // framebuffers and textures are really good friends
    // they do everything together
    // they grew up together
    // had kids together
    // grew old together
    // and on texture's death bed, with his last breath,
    // framebuffer said "I was always your friend..
    // but you were never mine. "
    friend class glFramebuffer;

    glTexture(GLenum pTarget) {
        mTarget = pTarget;
        glGenTextures(1, &mID);
    }

    ~glTexture() {
        glDeleteTextures(1, &mID);
    }

    void bind() {
        glBindTexture(mTarget, mID);
    }

    void unbind() {
        glBindTexture(mTarget, 0);
    }

    void bindToSlot(uint8_t slot) {
        glBindTextureUnit(slot, mID);
    }

    void genMipMaps() {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    void setFilter(Sampling::Filter filter) {
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

    void setWrap(Sampling::Wrap mode) {
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

    ImTextureID ImGuiID() { return (void*)((intptr_t)mID); }

protected:
    GLenum mTarget;
    unsigned int mID;
};

class glTexture2D : public glTexture {
public:
    glTexture2D() : glTexture(GL_TEXTURE_2D) {}

    void init(uint32_t width, uint32_t height, const Format::Format& format, const void* data = nullptr) {
        glTexImage2D(mTarget, 0, format.intFormat, width, height, 0, format.extFormat, format.type, data);
    }
};

class glTextureCube : public glTexture {
public:
    glTextureCube() : glTexture(GL_TEXTURE_CUBE_MAP) {}

    void init(uint32_t width, uint32_t height, uint8_t face, const Format::Format& format, const void* data = nullptr) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, format.intFormat, width, height, 0, format.extFormat, format.type, data);
    }


};



} //Namespace Raekor
#pragma once

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
        GLenum intFormat    = GL_INVALID_ENUM;
        GLenum extFormat    = GL_INVALID_ENUM;
        GLenum type         = GL_INVALID_ENUM;
    };

    static constexpr Format DEPTH           { GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT };
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

    // we don't allow copying of OpenGL wrapper classes
    // see section 2.1 of https://www.khronos.org/opengl/wiki/Common_Mistakes
    glTexture(const glTexture&) = delete;
    glTexture& operator=(const glTexture&) = delete;

    glTexture(glTexture&& other) {
        mID = other.mID;
        other.mID = 0; //Use null texture for the old object
    }

    glTexture& operator=(glTexture&& other) {
        if (this != &other) {
            glDeleteTextures(1, &mID);
            mID = 0;
            std::swap(mID, other.mID);
        }
    }

    operator GLuint() const { return mID; }

    void bind();
    void unbind();
    void bindToSlot(uint8_t slot);

    void genMipMaps();
    void setFilter(Sampling::Filter filter);
    void setWrap(Sampling::Wrap mode);

    ImTextureID ImGuiID();

    void clear(const glm::vec4& colour);

    unsigned int mID;
protected:
    GLenum mTarget;
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
    void init(uint32_t width, uint32_t height, uint32_t depth, GLenum format, const void* data);
    
    // oh C++, why are you like this ??
    using glTexture::bindToSlot;
    void bindToSlot(uint32_t slot, GLenum access, GLenum format);
};



} //Namespace Raekor
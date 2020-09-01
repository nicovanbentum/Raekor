#pragma once

namespace Raekor {

//////////////////////////////////////////////////////////////////////////////////

class OGLTexture {
public:
    OGLTexture(const OGLTexture& rhs) = delete;
    OGLTexture& operator=(const OGLTexture& rhs) = delete;

    OGLTexture(OGLTexture&& rhs) noexcept : handle(std::exchange(rhs.handle, 0)) {}

    OGLTexture& operator=(OGLTexture&& rhs) noexcept {
        glDeleteTextures(1, &handle);
        handle = std::exchange(rhs.handle, 0);
        return *this;
    }

    OGLTexture() = default;
    OGLTexture(GLenum target) { create(target); }

    ~OGLTexture() {
        glDeleteTextures(1, &handle);
    }

    void create(GLenum target) {
        glCreateTextures(target, 1, &handle);
    }

private:
    GLuint handle;
};

//////////////////////////////////////////////////////////////////////////////////

} //Namespace Raekor
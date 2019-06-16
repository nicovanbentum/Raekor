#include "pch.h"
#include "util.h"
#include "texture.h"

namespace Raekor {

Texture * Texture::construct(const std::string& path) {
    return new GLTexture(path);
}

GLTexture::GLTexture(const std::string& path)
{
    filepath = path;
    SDL_Surface* texture = SDL_LoadBMP(path.c_str());
    m_assert(texture != NULL, "failed to load BMP, wrong path?");

    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    int mode = GL_RGB;
    if(texture->format->BytesPerPixel == 4)  mode = GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, mode, texture->w, texture->h, 
                        0, mode, GL_UNSIGNED_BYTE, texture->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    SDL_FreeSurface(texture);
}

GLTexture::~GLTexture() {
    glDeleteTextures(1, &id);
}

void GLTexture::bind() const {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, id);
}

void GLTexture::unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}



} // Namespace Raekor
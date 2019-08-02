#include "pch.h"
#include "util.h"
#include "texture.h"

namespace Raekor {

Texture* Texture::construct(const std::string& path) {
    return new GLTexture(path);
}

Texture* Texture::construct(const std::vector<std::string>& face_files) {
    if(face_files.size() != 6) {
        return nullptr;
    }
    return new GLTextureCube(face_files);
}

GLTexture::GLTexture(const std::string& path)
{
    filepath = path;
    
    int w, h, ch;
    auto image = stbi_load(path.c_str(), &w, &h, &ch, 3);
    m_assert(image, "failed to load image");

    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 
                        0, GL_RGB, GL_UNSIGNED_BYTE, image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(image);
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

GLTextureCube::GLTextureCube(const std::vector<std::string>& face_files) {
    m_assert(face_files.size() == 6, std::string("texture cubes require 6 faces instead of " + face_files.size()));

    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, id);

    // for every face file we generate an OpenGL texture image
    int width, height, n_channels;
    for(unsigned int i = 0; i < face_files.size(); i++) {
        auto image = stbi_load(face_files[i].c_str(), &width, &height, &n_channels, 0);
        m_assert(image, "failed to load image");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        stbi_image_free(image);
    }

    // set the text parameters
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

GLTextureCube::~GLTextureCube() {
    glDeleteTextures(1, &id);
}

void GLTextureCube::bind() const {
    glBindTexture(GL_TEXTURE_CUBE_MAP, id);
}

void GLTextureCube::unbind() const {
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

} // Namespace Raekor
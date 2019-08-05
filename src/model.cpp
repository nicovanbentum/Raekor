#include "pch.h"
#include "model.h"

namespace Raekor {

TexturedModel::TexturedModel(const std::string& m_file, const std::string& tex_file): 
    mesh(nullptr), texture(nullptr) {
    if(!m_file.empty()) {
        mesh.reset(new Mesh(m_file));
    }
    if(!tex_file.empty()) {
        texture.reset(Texture::construct(tex_file));
    }
}

void TexturedModel::set_mesh(const std::string& m_file) {
    mesh.reset(new Mesh(m_file));
}

void TexturedModel::set_texture(const std::string& tex_file) {
    texture.reset(Texture::construct(tex_file));
}

void TexturedModel::bind() const {

    // if we didn't load the texture yet we only bind the mesh
    if(texture == nullptr ? true : false) {
        mesh->bind();
        return;
    }
    texture->bind();
}

void TexturedModel::unbind() const {
    if (texture == nullptr) {
        return;
    }
    texture->unbind();
}

} // Namespace Raekor
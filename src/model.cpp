#include "pch.h"
#include "model.h"

namespace Raekor {

TexturedModel::TexturedModel(const std::string& m_file, const std::string& tex_file): 
    mesh(nullptr), texture(nullptr) {
    if(!m_file.empty()) {
        mesh.reset(new Mesh(m_file, Mesh::file_format::OBJ));
    }
    if(!tex_file.empty()) {
        texture.reset(Texture::construct(tex_file));
    }
}

void TexturedModel::set_mesh(const std::string& m_file) {
    mesh.reset(new Mesh(m_file, Mesh::file_format::OBJ));
}

void TexturedModel::set_texture(const std::string& tex_file) {
    texture.reset(Texture::construct(tex_file));
}

void TexturedModel::render() const {

    // if we didn't load the texture yet we only render the mesh
    if(texture == nullptr ? true : false) {
        mesh->render();
        return;
    }

    // if we did load the texture we bind it for rendering
    texture->bind();
    mesh->render();
    texture->unbind();
}

} // Namespace Raekor
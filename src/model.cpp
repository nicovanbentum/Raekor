#include "pch.h"
#include "model.h"

namespace Raekor {

TexturedModel::TexturedModel(const std::string& m_file, const std::string& tex_file){
    mesh.reset(new Mesh(m_file, Mesh::file_format::OBJ));
    texture.reset(Texture::construct(tex_file));
}

void TexturedModel::set_mesh(const std::string& m_file) {
    mesh.reset(new Mesh(m_file, Mesh::file_format::OBJ));
}

void TexturedModel::set_texture(const std::string& tex_file) {
    texture.reset(Texture::construct(tex_file));
}

void TexturedModel::render() const {
    texture->bind();
    mesh->render();
    texture->unbind();
}

} // Namespace Raekor
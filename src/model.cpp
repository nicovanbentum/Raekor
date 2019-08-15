#include "pch.h"
#include "model.h"

namespace Raekor {

Model::Model(const std::string& m_file, const std::string& tex_file): 
    mesh(nullptr), texture(nullptr) {
    if(!m_file.empty()) {
        mesh.reset(new Mesh(m_file));
    }
    if(!tex_file.empty()) {
        texture.reset(Texture::construct(tex_file));
    }
}

void Model::set_mesh(const std::string& m_file) {
    mesh.reset(new Mesh(m_file));
}

void Model::set_texture(const std::string& tex_file) {
    texture.reset(Texture::construct(tex_file));
}

void Model::bind() const {
    if (texture) texture->bind();
    if (mesh) mesh->bind();
}

} // Namespace Raekor
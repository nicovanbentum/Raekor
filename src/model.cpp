#include "pch.h"
#include "model.h"

namespace Raekor {

Model::Model(const std::string& m_file, const std::string& tex_file): 
    mesh(nullptr), texture(nullptr) {
    transform = glm::mat4(1.0f);
    scale = glm::vec3(1.0f);
    position = glm::vec3(0.0f);
    rotation = glm::vec3(0.0f);

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

Model::operator bool() const {
    return mesh != nullptr;
}

void Model::reset_transform() {
    transform = glm::mat4(1.0f);
    scale = glm::vec3(1.0f);
    position = glm::vec3(0.0f);
    rotation = glm::vec3(0.0f);
}

void Model::recalc_transform() {
    transform = glm::mat4(1.0f);
    transform = glm::translate(transform, position);
    auto rotation_quat = static_cast<glm::quat>(rotation);
    transform = transform * glm::toMat4(rotation_quat);
    transform = glm::scale(transform, scale);
}


} // Namespace Raekor
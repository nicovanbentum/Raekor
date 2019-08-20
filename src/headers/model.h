#pragma once

#include "pch.h"
#include "mesh.h"
#include "texture.h"

namespace Raekor {

class Model {

public:
    Model(const std::string& m_file = "", const std::string& tex_file = "");

    void set_mesh(const std::string& m_file);
    void set_texture(const std::string& tex_file);
    bool has_texture() const { return texture != nullptr; }

    void reset_transform();
    void recalc_transform();

    inline  Mesh* get_mesh() const { return mesh.get(); }
    inline  Texture* get_texture() const { return texture.get(); }
    
    inline glm::mat4& get_transform() { return transform; }

    inline float* scale_ptr() { return &scale.x; }
    inline float* pos_ptr() { return &position.x; }
    inline float* rotation_ptr() { return &rotation.x; }

    void bind() const;

    // explicit to avoid implicit bool conversions
    explicit operator bool() const;

protected:
    glm::mat4 transform;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;

    std::unique_ptr<Mesh> mesh;
    std::unique_ptr<Texture> texture;
};

} // Namespace Raekor
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

    inline  Mesh* get_mesh() const { return mesh.get(); }
    inline  Texture* get_texture() const { return texture.get(); }

    void bind() const;

    // explicit to avoid implicit bool conversions
    explicit operator bool() const;

protected:
    std::unique_ptr<Mesh> mesh;
    std::unique_ptr<Texture> texture;
};

} // Namespace Raekor
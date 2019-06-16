#pragma once

#include "pch.h"
#include "mesh.h"
#include "texture.h"

namespace Raekor {

class TexturedModel {

public:
    TexturedModel(const std::string& m_file, const std::string& tex_file);
    ~TexturedModel();

    void set_mesh(const std::string& m_file);
    void set_texture(const std::string& tex_file);

    inline const Mesh& get_mesh() const { return *mesh; }
    inline const Texture& get_texture() const { return *texture; }

    void render() const;

protected:
    std::unique_ptr<Mesh> mesh;
    std::unique_ptr<Texture> texture;
};

} // Namespace Raekor
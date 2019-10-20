#pragma once

#include "pch.h"
#include "mesh.h"
#include "texture.h"

#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/Importer.hpp"

namespace Raekor {

class Model {

public:
    Model(const std::string& m_file = "");

    bool complete() const;
    void load_from_disk();
    void load_mesh(uint64_t index);
    void reload();
    inline void set_path(const std::string& new_path) { path = new_path; }
    inline std::string get_path() { return path; }

    void reset_transform();
    void recalc_transform();
    
    inline glm::mat4& get_transform() { return transform; }

    inline float* scale_ptr() { return &scale.x; }
    inline float* pos_ptr() { return &position.x; }
    inline float* rotation_ptr() { return &rotation.x; }
    inline unsigned int mesh_count() { return static_cast<unsigned int>(meshes.size()); }

    void render() const;

    std::optional<const Mesh*> operator[](unsigned int index) {
        if (index < meshes.size()) {
            return &meshes[index];
        }
        return {};
    }

protected:
    glm::mat4 transform;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;

    std::string path;
    std::vector<Mesh> meshes;
    std::vector<std::shared_ptr<Texture>> textures;

private:
    const aiScene* scene;
    std::vector<std::future<void>> futures;
};

} // Namespace Raekor
#pragma once

#include "pch.h"
#include "buffer.h"
#include "buffer.h"

namespace Raekor {

class Mesh {
public:
    Mesh(const std::string& filepath);
    
    void reset_transform();
    void recalc_transform();

    inline float* scale_ptr() { return &scale.x; }
    inline float* pos_ptr() { return &position.x; }
    inline float* rotation_ptr() { return &rotation.x; }

    inline glm::mat4& get_transform() { return transform; }
    inline std::string get_mesh_path() { return mesh_path; }

    const VertexBuffer* get_vertex_buffer() const { return vb.get(); }
    const IndexBuffer* get_index_buffer() const { return ib.get(); }

    void bind() const;
    void load_data();

    // new for assimp model extraction
    std::vector<Vertex> vertexes;
    std::vector<Index> indexes;
private:
    // TODO: move these into some sort of object class for our scene structure
    glm::mat4 transform;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;

    std::string mesh_path;

    std::unique_ptr<VertexBuffer> vb;
    std::unique_ptr<IndexBuffer> ib;
};

}
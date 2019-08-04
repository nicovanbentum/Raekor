#pragma once

#include "pch.h"
#include "buffer.h"
#include "renderer.h"

namespace Raekor {

struct Vertex {
    glm::vec3 pos;
    glm::vec2 uv;
    glm::vec3 normal;

    bool operator<(const Raekor::Vertex & rhs) const {
        return memcmp((void*)this, (void*)&rhs, sizeof(Raekor::Vertex)) > 0;
    };

    bool operator>(const Raekor::Vertex & rhs) const {
        return memcmp((void*)this, (void*)&rhs, sizeof(Raekor::Vertex)) < 0;
    };

};

class Mesh {
public:
    enum class file_format {
        OBJ, FBX
    };

public:
    Mesh(const std::string& filepath, Mesh::file_format format);
    
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
    void load_data(const std::string& filepath);
    bool parse_OBJ(const std::string& filepath);

    std::vector<Vertex> vertexes;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> vertices;
    std::vector<unsigned int> indices;

private:
    glm::mat4 transform;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;

    std::string mesh_path;

    std::unique_ptr<VertexBuffer> vb;
    std::unique_ptr<IndexBuffer> ib;
};

}
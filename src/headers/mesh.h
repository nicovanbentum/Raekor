#pragma once

#include "pch.h"

namespace Raekor {

struct Vertex {
    glm::vec3 pos;
    glm::vec2 uv;
    glm::vec2 normal;

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
    
    bool parse_OBJ(const std::string& filepath);
    virtual void render();

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

    unsigned int vertexbuffer;
    unsigned int uvbuffer;
    unsigned int elementbuffer;
};

}
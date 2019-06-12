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
    Mesh(std::string& filepath, Mesh::file_format format);
    Mesh(std::string& filepath);
    Mesh() : file_path("None") {}
    
    inline std::string get_file_path() { return file_path; }
    inline glm::mat4& get_transformation() { return transformation; }
    
    void move(const glm::vec3& delta);
    void scale(const glm::vec3& factor);
    void rotate(const glm::mat4& rotation);
    inline void reset_transformation() { transformation = glm::mat4(1.0f); }
    
    bool parse_OBJ(std::string& filepath);    
    void render();

public:
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> vertices;
    std::vector<unsigned int> indices;

    glm::mat4 transformation = glm::mat4(1.0f);

    std::string file_path;

    unsigned int vertexbuffer;
    unsigned int uvbuffer;
    unsigned int elementbuffer;

};

}
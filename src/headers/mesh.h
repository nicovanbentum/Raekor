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
    Mesh(const std::string& filepath);
    Mesh() : mesh_path("None") {}
    Mesh(std::string& filepath, Mesh::file_format format);
    
    glm::mat4 get_rotation_matrix();
    inline std::string get_mesh_path() { return mesh_path; }
    inline glm::mat4& get_transformation() { return transformation; }
    
    void reset_transformation();
    void move(const glm::vec3& delta);
    void scale_by(const glm::vec3& factor);
    void rotate(const glm::mat4& rotation);
    
    bool parse_OBJ(std::string& filepath);    
    virtual void render();

public:
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> vertices;
    std::vector<unsigned int> indices;

    glm::mat4 transformation;

    std::string mesh_path;

    unsigned int vertexbuffer;
    unsigned int uvbuffer;
    unsigned int elementbuffer;

    float scale;
	glm::vec3 position;
    glm::vec3 euler_rotation;
};

class TexturedMesh : public Raekor::Mesh {
public:
    TexturedMesh(std::string& obj, const std::string& image, Mesh::file_format format);
    TexturedMesh(std::string& obj, Mesh::file_format format);
    void load_texture(const std::string& path);
    void render(int sampler_id);
    
public:   
    std::string image_path;
    unsigned int texture_id;


};

}
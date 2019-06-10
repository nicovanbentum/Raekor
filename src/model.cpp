#include "pch.h"
#include "model.h"
#include "util.h"

bool GE_load_obj(const char* path,
                std::vector<glm::vec3>& out_vertices,
                std::vector<glm::vec2>& out_uvs,
                std::vector<glm::vec3>& out_normals) {
    std::vector<unsigned int> v_indices, u_indices, n_indices;
    std::vector<glm::vec3> verts, norms;
    std::vector<glm::vec2> uvs;

    std::fstream file(path, std::ios::in);
    if (!file.is_open())
        return false;

    std::string buffer;
    std::istringstream ss;

    while (std::getline(file, buffer)) {
        std::istringstream line(buffer);
        std::string token;
        line >> token;

        if (token == "v") {
            glm::vec3 vertex;
            line >> vertex.x >> vertex.y >> vertex.z;
            verts.push_back(vertex);
        } 
        else if (token == "vt") {
            glm::vec2 uv;
            line >> uv.x >> uv.y;
            uvs.push_back(uv);
        } 
        else if (token == "vn") {
            glm::vec3 norm;
            line >> norm.x >> norm.y >> norm.z;
            norms.push_back(norm);
        } 
        else if (token == "f") {
            std::string face;
            while (line >> face) {
                std::replace(face.begin(), face.end(), '/', ' ');
                std::istringstream spaced_face(face);
                unsigned int vi = 0, ui = 0, ni = 0;
                spaced_face >> vi >> ui >> ni;
                v_indices.push_back(vi);
                u_indices.push_back(ui);
                n_indices.push_back(ni);
            }
        }
    }

    for (auto& vi : v_indices) {
        out_vertices.push_back(verts[vi - 1]);
    }
    for (auto& ui : u_indices) {
        out_uvs.push_back(uvs[ui - 1]);
    }
    for (auto& ni : n_indices) {
        out_normals.push_back(norms[ni - 1]);
    }
    return true;
}

unsigned int find_index(std::map<packed_vertex, unsigned int>& vi,
                            packed_vertex& pv) {
    auto it = vi.find(pv);
    if (it == vi.end())
        return -1;
    else
        return it->second;
}

void index_model_vbo(GE_model& m) {
    std::map<packed_vertex, unsigned int> vertex_indices;

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;

    for (unsigned int i = 0; i < m.vertices.size(); i++) {
        packed_vertex pv = {m.vertices[i], m.uvs[i], m.normals[i]};

        auto index = find_index(vertex_indices, pv);
        if (index == -1) {
            vertices.push_back(m.vertices[i]);
            uvs.push_back(m.uvs[i]);
            normals.push_back(m.normals[i]);
            auto new_index = static_cast<unsigned int>(vertices.size() - 1);
            m.indices.push_back(new_index);
        } else
            m.indices.push_back(index);
    }
}
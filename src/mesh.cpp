#include "pch.h"
#include "mesh.h"
#include "util.h"

namespace Raekor {

Mesh::Mesh(std::string& filepath, Mesh::file_format format) :
file_path(filepath) {

    switch(format) {
        case Mesh::file_format::OBJ : {
            m_assert(parse_OBJ(filepath), "failed to load obj");
        } break;
        case Mesh::file_format::FBX : {
            // TODO: implement different file formats, thus moving it to its own class
        }
    }
}

Mesh::Mesh(std::string& filepath) :
file_path(filepath) {}

void Mesh::scale(const glm::vec3& factor) {
    transformation = glm::scale(transformation, factor);
}

void Mesh::move(const glm::vec3& delta) {
    transformation = glm::translate(transformation, delta);
}

void Mesh::rotate(const glm::mat4& rotation) {
    transformation = transformation * rotation;
}

bool Mesh::parse_OBJ(std::string& filepath) {
    std::vector<unsigned int> v_indices, u_indices, n_indices;
    std::vector<glm::vec3> v_verts, v_norms;
    std::vector<glm::vec2> v_uvs;

    std::fstream file(filepath, std::ios::in);
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
            v_verts.push_back(vertex);
        } 
        else if (token == "vt") {
            glm::vec2 uv;
            line >> uv.x >> uv.y;
            v_uvs.push_back(uv);
        } 
        else if (token == "vn") {
            glm::vec3 norm;
            line >> norm.x >> norm.y >> norm.z;
            v_norms.push_back(norm);
        } 
        else if (token == "f") {
            std::string face;
            while (line >> face) {
                std::replace(face.begin(), face.end(), '/', ' ');
                std::istringstream spaced_face(face);
                unsigned int vi = 0, ui = 0, ni = 0;
                spaced_face >> vi >> ui >> ni;
                if(!ni) {
                    ni = ui;
                    ui = 0;
                }

                v_indices.push_back(vi);
                if (ui) {
                    u_indices.push_back(ui);
                }
                n_indices.push_back(ni);
            }
        }
    }


    for (auto& vi : v_indices) {
        vertices.push_back(v_verts[vi - 1]);
    }
    for (auto& ui : u_indices) {
        uvs.push_back(v_uvs[ui - 1]);
    }
    for (auto& ni : n_indices) {
        normals.push_back(v_norms[ni - 1]);
}

    std::map<Raekor::Vertex, unsigned int> vi;

    std::vector<glm::vec3> i_vertices;
    std::vector<glm::vec2> i_uvs;
    std::vector<glm::vec3> i_normals;

    for (unsigned int i = 0; i < vertices.size(); i++) {
        
        Raekor::Vertex pv = {vertices[i], uvs[i], normals[i]};

        auto index = vi.find(pv);
        if (index == vi.end()) {
            i_vertices.push_back(vertices[i]);
            i_uvs.push_back(uvs[i]);
            i_normals.push_back(normals[i]);
            auto new_index = static_cast<unsigned int>(i_vertices.size() - 1);
            indices.push_back(new_index);
        } else
            indices.push_back(index->second);
    }
    
    // generate the openGL buffers and get ID's
    vertexbuffer = gen_gl_buffer(vertices, GL_ARRAY_BUFFER);
    uvbuffer = gen_gl_buffer(uvs, GL_ARRAY_BUFFER);
    elementbuffer = gen_gl_buffer(indices, GL_ELEMENT_ARRAY_BUFFER);

    return true;
}

void Mesh::render() {
    //set attribute buffer for model vertices
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0,  (void*)0 );

    //set attribute buffer for uvs
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

    // Draw triangles
    glDrawElements(GL_TRIANGLES, static_cast<int>(indices.size()), GL_UNSIGNED_INT, nullptr);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

}
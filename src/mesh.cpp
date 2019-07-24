#include "pch.h"
#include "mesh.h"
#include "util.h"

namespace Raekor {

Mesh::Mesh(const std::string& filepath, Mesh::file_format format) :
mesh_path(filepath) {

    switch(format) {
        case Mesh::file_format::OBJ : {
            m_assert(parse_OBJ(filepath), "failed to load obj");
        } break;
        case Mesh::file_format::FBX : {
            // TODO: implement different file formats, thus moving it to its own class
        }
    }
    transform = glm::mat4(1.0f);
    scale = glm::vec3(1.0f);
    position = glm::vec3(0.0f);
    rotation = glm::vec3(0.0f);
}

void Mesh::reset_transform() {
    transform = glm::mat4(1.0f);
    scale = glm::vec3(1.0f);
    position = glm::vec3(0.0f);
    rotation = glm::vec3(0.0f);
}

void Mesh::recalc_transform() {
    transform = glm::mat4(1.0f);
    transform = glm::translate(transform, position);
    auto rotation_quat = static_cast<glm::quat>(rotation);
    transform = transform * glm::toMat4(rotation_quat);
    transform = glm::scale(transform, scale);
}

bool Mesh::parse_OBJ(const std::string& filepath) {
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

    // TODO: sort out the mess that follows
    for (auto& vi : v_indices) {
        vertices.push_back(v_verts[vi - 1]);
    }
    for (auto& ui : u_indices) {
        uvs.push_back(v_uvs[ui - 1]);
    }
    for (auto& ni : n_indices) {
        normals.push_back(v_norms[ni - 1]);
}

    // indexing algorithm
    std::map<Raekor::Vertex, unsigned int> seen;
    for (unsigned int i = 0; i < vertices.size(); i++) {
        Raekor::Vertex pv = {vertices[i], uvs[i], normals[i]};

        // this only works because we added > and < operators to Vertex
        auto index = seen.find(pv);

        // if we didn't find it, add the vertex to seen and push its index
        if (index == seen.end()) {
            indices.push_back(i);
            seen.insert(std::make_pair(pv, i));
        } else { // if we did find it, we push the existing index
            indices.push_back(index->second);
        }
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    //set attribute buffer for uvs
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // bind our index buffer (can't believe I forgot this line for so long lol)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);

    // Draw triangles
    glDrawElements(GL_TRIANGLES, static_cast<int>(indices.size()), GL_UNSIGNED_INT, nullptr);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

} // Namespace Raekor
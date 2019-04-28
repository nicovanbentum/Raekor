#pragma once

#include "pch.h"

//data structures
struct GE_model {
    unsigned int vertices_id;
    unsigned int uvs_id;
    unsigned int normals_id;
    unsigned int indices_id;

    std::vector<vec3> vertices;
    std::vector<vec2> uvs;
    std::vector<vec3> normals;
    std::vector<unsigned int> indices;

    mat4 transformation = mat4(1.0f);
};

struct packed_vertex {
    vec3 pos;
    vec2 uv;
    vec3 normal;

    bool operator<(const packed_vertex & rhs) const {
        return memcmp((void*)this, (void*)&rhs, sizeof(packed_vertex)) > 0;
    };

    bool operator>(const packed_vertex & rhs) const {
        return memcmp((void*)this, (void*)&rhs, sizeof(packed_vertex)) < 0;
    };
};

//functions
bool GE_load_obj(
    const char * path,
    std::vector < glm::vec3 > & out_vertices,
    std::vector < glm::vec2 > & out_uvs,
    std::vector < glm::vec3 > & out_normals);

void index_model_vbo(GE_model & m);
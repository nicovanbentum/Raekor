#include "pch.h"
#include "mesh.h"
#include "util.h"
#include "renderer.h"
#include "buffer.h"

#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/Importer.hpp"

namespace Raekor {

std::vector<Vertex> v_cube = {
    {{-.5f, -.5f, -.5f}, {}, {}, {}, {}},
    {{.5f, -.5f, -.5f}, {}, {}, {}, {}},
    {{.5f, .5f, -.5f}, {}, {}, {}, {}},
    {{-.5f, .5f, -.5f}, {}, {}, {}, {}},

    {{-.5f, -.5f, .5f}, {}, {}, {}, {}},
    {{.5f, -.5f, .5f}, {}, {}, {}, {}},
    {{.5f, .5f, .5f}, {}, {}, {}, {}},
    {{-.5f, .5f, .5f}, {}, {}, {}, {}}
};

std::vector<Index>  i_cube = {
    {0, 1, 3}, {3, 1, 2},
    {1, 5, 2}, {2, 5, 6},
    {5, 4, 6}, {6, 4, 7},
    {4, 0, 7}, {7, 0, 3},
    {3, 2, 7}, {7, 2, 6},
    {4, 5, 0}, {0, 5, 1}
};

std::vector<Vertex> v_quad = {
    {{-1.0f, -1.0f, 0.0f},  {0.0f, 0.0f},   {}, {}, {}},
    {{1.0f, -1.0f, 0.0f},   {1.0f, 0.0f},   {}, {}, {}},
    {{1.0f, 1.0f, 0.0f},    {1.0f, 1.0f},   {}, {}, {}},
    {{-1.0f, 1.0f, 0.0f},   {0.0f, 1.0f},   {}, {}, {}}
};

std::vector<Index> i_quad = {
    {0, 1, 3}, {3, 1, 2}
};


Mesh::Mesh(Shape basic_shape) {
    switch (basic_shape) {
    case Shape::None: return;
    case Shape::Cube: {
        set_vertex_buffer(v_cube);
        set_index_buffer(i_cube);
    } break;
    case Shape::Quad: {
        set_vertex_buffer(v_quad);
        set_index_buffer(i_quad);
    } break;
    }
}

Mesh::Mesh(const std::vector<Vertex>& vb, const std::vector<Index>& ib) {
    set_vertex_buffer(vb);
    set_index_buffer(ib);
}

void Mesh::set_vertex_buffer(const std::vector<Vertex>& buffer) {
    vb.reset(Raekor::VertexBuffer::construct(buffer));
}
void Mesh::set_index_buffer(const std::vector<Index>& buffer) {
    ib.reset(Raekor::IndexBuffer::construct(buffer));
}

void Mesh::bind() const {
    vb->bind();
    ib->bind();
}

} // Namespace Raekor
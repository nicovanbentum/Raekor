#include "pch.h"
#include "mesh.h"
#include "util.h"
#include "renderer.h"
#include "buffer.h"

#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/Importer.hpp"

namespace Raekor {

// cube without UV's
static std::vector<Vertex> v_cube = {
    {{.5f, .5f, .5f}, {}, {}},  {{-.5f, .5f, .5f}, {}, {}},  {{-.5f,-.5f, .5f}, {}, {}},  {{.5f,-.5f, .5f}, {}, {}},
    {{.5f, .5f, .5f}, {}, {}},   {{.5f,-.5f, .5f}, {}, {}},   {{.5f,-.5f,-.5f}, {}, {}},  {{.5f, .5f,-.5f}, {}, {}},
    {{.5f, .5f, .5f}, {}, {}},   {{.5f, .5f,-.5f}, {}, {}},  {{-.5f, .5f,-.5f}, {}, {}}, {{-.5f, .5f, .5f}, {}, {}},
    {{-.5f, .5f, .5f}, {}, {}},  {{-.5f, .5f,-.5f}, {}, {}},  {{-.5f,-.5f,-.5f}, {}, {}}, {{-.5f,-.5f, .5f}, {}, {}},
    {{-.5f,-.5f,-.5f}, {}, {}},   {{.5f,-.5f,-.5f}, {}, {}},   {{.5f,-.5f, .5f}, {}, {}}, {{-.5f,-.5f, .5f}, {}, {}},
    {{ .5f,-.5f,-.5f}, {}, {}},  {{-.5f,-.5f,-.5f}, {}, {}},  {{-.5f, .5f,-.5f}, {}, {}},  {{.5f, .5f,-.5f}, {}, {}}
};

static std::vector<Index>  i_cube = {
    {0, 1, 2},   {2, 3, 0},
    {4, 5, 6},   {6, 7, 4},
    {8, 9,10},  {10,11, 8},
    {12,13,14},  {14,15,12},
    {16,17,18},  {18,19,16},
    {20,21,22},  {22,23,20}
};

Mesh::Mesh(Shape basic_shape) {
    switch (basic_shape) {
    case Shape::None: return;
    case Shape::Cube: {
        set_vertex_buffer(v_cube);
        set_index_buffer(i_cube);
    }
    }
}

Mesh::Mesh(Mesh&& rhs) {
    name = rhs.name;
    vb = std::move(rhs.vb);
    ib = std::move(rhs.ib);
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
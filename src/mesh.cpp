#include "pch.h"
#include "mesh.h"
#include "util.h"
#include "renderer.h"
#include "buffer.h"

#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/Importer.hpp"

namespace Raekor {

std::vector<Vertex> cubeVertices = {
    {{-1.0f, -1.0f, -1.0f}, {}, {}, {}, {}},
    {{1.0f, -1.0f, -1.0f}, {}, {}, {}, {}},
    {{1.0f, 1.0f, -0.999999f}, {}, {}, {}, {}},
    {{-1.0f, 1.0f, -1.0f}, {}, {}, {}, {}},

    {{-1.0f, -1.0f, 1.0f}, {}, {}, {}, {}},
    {{1.0f, -1.0f, 1.0f}, {}, {}, {}, {}},
    {{0.999999f, 1.0f, 1.000001f}, {}, {}, {}, {}},
    {{-1.0f, 1.0f, 1.0f}, {}, {}, {}, {}}
};

std::vector<Index>  cubeIndices = {
    {0, 1, 3}, {3, 1, 2},
    {1, 5, 2}, {2, 5, 6},
    {5, 4, 6}, {6, 4, 7},
    {4, 0, 7}, {7, 0, 3},
    {3, 2, 7}, {7, 2, 6},
    {4, 5, 0}, {0, 5, 1}
};

std::vector<Vertex> quadVertices = {
    {{-1.0f, -1.0f, 0.0f},  {0.0f, 0.0f},   {}, {}, {}},
    {{1.0f, -1.0f, 0.0f},   {1.0f, 0.0f},   {}, {}, {}},
    {{1.0f, 1.0f, 0.0f},    {1.0f, 1.0f},   {}, {}, {}},
    {{-1.0f, 1.0f, 0.0f},   {0.0f, 1.0f},   {}, {}, {}}
};

std::vector<Index> quadIndices = {
    {0, 1, 3}, {3, 1, 2}
};


Mesh::Mesh(Shape shape) {
    switch (shape) {
    case Shape::None: return;
    case Shape::Cube: {
        setVertexBuffer(cubeVertices);
        setIndexBuffer(cubeIndices);
    } break;
    case Shape::Quad: {
        setVertexBuffer(quadVertices);
        setIndexBuffer(quadIndices);
    } break;
    }
}

Mesh::Mesh(const std::vector<Vertex>& vb, const std::vector<Index>& ib) {
    setVertexBuffer(vb);
    setIndexBuffer(ib);
}

void Mesh::setVertexBuffer(const std::vector<Vertex>& buffer) {
    vb.reset(Raekor::VertexBuffer::construct(buffer));
}
void Mesh::setIndexBuffer(const std::vector<Index>& buffer) {
    ib.reset(Raekor::IndexBuffer::construct(buffer));
}

void Mesh::bind() const {
    vb->bind();
    ib->bind();
}

} // Namespace Raekor
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

std::vector<Vertex> unitCubeVertices = {
    {{0.0f, 0.0f, 0.0f}, {}, {}, {}, {}},
    {{1.0f, 0.0f, 0.0f}, {}, {}, {}, {}},
    {{1.0f, 1.0f, 0.0f}, {}, {}, {}, {}},
    {{0.0f, 1.0f, 0.0f}, {}, {}, {}, {}},

    {{0.0f, 0.0f, 1.0f}, {}, {}, {}, {}},
    {{1.0f, 0.0f, 1.0f}, {}, {}, {}, {}},
    {{1.0f, 1.0f, 1.0f}, {}, {}, {}, {}},
    {{0.0f, 1.0f, 1.0f}, {}, {}, {}, {}}
};

std::vector<Triangle>  cubeIndices = {
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

std::vector<Triangle> quadIndices = {
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

Mesh::Mesh(std::vector<Vertex>& vb, std::vector<Triangle>& ib) {
    setVertexBuffer(vb);
    setIndexBuffer(ib);
}

void Mesh::setVertexBuffer(std::vector<Vertex>& buffer) {
    vb.reset(new glVertexBuffer());
    vb->loadVertices(buffer.data(), buffer.size());
}
void Mesh::setIndexBuffer(std::vector<Triangle>& buffer) {
    ib.reset(new glIndexBuffer());
    ib->loadFaces(buffer.data(), buffer.size());
}

void Mesh::bind() const {
    vb->bind();
    ib->bind();
}

} // Namespace Raekor
#include "pch.h"
#include "mesh.h"
#include "util.h"
#include "renderer.h"
#include "buffer.h"

#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/Importer.hpp"

namespace Raekor {

Mesh::Mesh(Shape shape) {
    switch (shape) {
    case Shape::None: return;
    case Shape::Cube: {
        setVertexBuffer(unitCubeVertices.data(), unitCubeVertices.size());
        setIndexBuffer(cubeIndices.data(), cubeIndices.size());
    } break;
    case Shape::Quad: {
        setVertexBuffer(quadVertices.data(), quadVertices.size());
        setIndexBuffer(quadIndices.data(), quadIndices.size());
    } break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

Mesh::Mesh(std::vector<Vertex>& vb, std::vector<Triangle>& ib) {
    setVertexBuffer(vb);
    setIndexBuffer(ib);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

Mesh::Mesh(const Vertex* vertices, size_t vSize, const Triangle* triangles, size_t tSize) {
    setVertexBuffer(vertices, vSize);
    setIndexBuffer(triangles, tSize);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::setVertexBuffer(std::vector<Vertex>& buffer) {
    vb.reset(new glVertexBuffer());
    vb->loadVertices(buffer.data(), buffer.size());
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::setVertexBuffer(const Vertex* data, size_t size) {
    vb.reset(new glVertexBuffer());
    vb->loadVertices(data, size);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::setIndexBuffer(std::vector<Triangle>& buffer) {
    ib.reset(new glIndexBuffer());
    ib->loadFaces(buffer.data(), buffer.size());
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::setIndexBuffer(const Triangle* data, size_t size) {
    ib.reset(new glIndexBuffer());
    ib->loadFaces(data, size);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::render() {
    bind();
    Renderer::DrawIndexed(ib->count);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Mesh> Mesh::createCube(const std::array<glm::vec3, 2>& bounds) {
    const auto min = bounds[0];
    const auto max = bounds[1];

    const std::vector<Vertex> vertices = {
        { { min } },
        { { max[0], min[1], min[2] } } ,
        { { max[0], max[1], min[2] } } ,
        { { min[0], max[1], min[2] } } ,

        { { min[0], min[1], max[2] } } ,
        { { max[0], min[1], max[2] } } ,
        { { max } },
        { { min[0], max[1], max[2] } }
    };

    return std::make_unique<Mesh>(vertices.data(), vertices.size(), cubeIndices.data(), cubeIndices.size());
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::bind() const {
    vb->bind();
    ib->bind();
}

} // Namespace Raekor
#pragma once

#include "buffer.h"

namespace Raekor {

constexpr std::array<Vertex, 8> unitCubeVertices = {
    Vertex{{0.0f, 0.0f, 0.0f}},
    Vertex{{1.0f, 0.0f, 0.0f}},
    Vertex{{1.0f, 1.0f, 0.0f}},
    Vertex{{0.0f, 1.0f, 0.0f}},
    Vertex{{0.0f, 0.0f, 1.0f}},
    Vertex{{1.0f, 0.0f, 1.0f}},
    Vertex{{1.0f, 1.0f, 1.0f}},
    Vertex{{0.0f, 1.0f, 1.0f}}
};

//////////////////////////////////////////////////////////////////////////////////////////////////

constexpr std::array<Triangle, 12>  cubeIndices = {
    Triangle{0, 1, 3}, Triangle{3, 1, 2},
    Triangle{1, 5, 2}, Triangle{2, 5, 6},
    Triangle{5, 4, 6}, Triangle{6, 4, 7},
    Triangle{4, 0, 7}, Triangle{7, 0, 3},
    Triangle{3, 2, 7}, Triangle{7, 2, 6},
    Triangle{4, 5, 0}, Triangle{0, 5, 1}
};

//////////////////////////////////////////////////////////////////////////////////////////////////

constexpr std::array<Vertex, 4> quadVertices = {
    Vertex{{-1.0f, -1.0f, 0.0f},  {0.0f, 0.0f},   {}, {}, {}},
    Vertex{{1.0f, -1.0f, 0.0f},   {1.0f, 0.0f},   {}, {}, {}},
    Vertex{{1.0f, 1.0f, 0.0f},    {1.0f, 1.0f},   {}, {}, {}},
    Vertex{{-1.0f, 1.0f, 0.0f},   {0.0f, 1.0f},   {}, {}, {}}
};

//////////////////////////////////////////////////////////////////////////////////////////////////

constexpr std::array<Triangle, 2> quadIndices = {
    Triangle{0, 1, 3}, Triangle{3, 1, 2}
};

//////////////////////////////////////////////////////////////////////////////////////////////////

enum class Shape {
    None, Cube, Quad
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class Mesh {
public:
    Mesh(Shape basic_shape = Shape::None);
    Mesh(std::vector<Vertex>& vb, std::vector<Triangle>& ib);
    Mesh(const Vertex* vertices, size_t vSize, const Triangle* triangles, size_t tSize);

    void setVertexBuffer(std::vector<Vertex>& buffer);
    void setVertexBuffer(const Vertex* data, size_t size);
    void setIndexBuffer(std::vector<Triangle>& buffer);
    void setIndexBuffer(const Triangle* data, size_t size);

    const glVertexBuffer* const getVertexBuffer() const { return vb.get(); }
    const glIndexBuffer* const getIndexBuffer() const { return ib.get(); }
    
    void render();

    static std::unique_ptr<Mesh> createCube(const std::array<glm::vec3, 2>& bounds);

    void bind() const;

protected:
    // TODO: we allow unnamed meshes, reconsider?
    std::unique_ptr<glVertexBuffer> vb;
    std::unique_ptr<glIndexBuffer> ib;
};

}
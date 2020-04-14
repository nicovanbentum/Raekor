#pragma once

#include "pch.h"
#include "buffer.h"

namespace Raekor {

extern std::vector<Vertex> cubeVertices;
extern std::vector<Index> cubeIndices;
extern std::vector<Vertex> quadVertices;
extern std::vector<Index> quadIndices;

enum class Shape {
    None, Cube, Quad
};

class Mesh {
public:
    Mesh(Shape basic_shape = Shape::None);
    Mesh(const std::vector<Vertex>& vb, const std::vector<Index>& ib);

    void setVertexBuffer(const std::vector<Vertex>& buffer);
    void setIndexBuffer(const std::vector<Index>& buffer);

    const VertexBuffer* const getVertexBuffer() const { return vb.get(); }
    const IndexBuffer* const getIndexBuffer() const { return ib.get(); }
    
    inline void render() {
        bind();
        Renderer::DrawIndexed(ib->getCount());
    }

    static Mesh* createCube(const std::array<glm::vec3, 2>& bounds) {
        const auto min = bounds[0];
        const auto max = bounds[1];

        std::vector<Vertex> vertices = {
            { {min} },
            { {max[0], min[1], min[2] } } ,
            { {max[0], max[1], min[2] } } ,
            { {min[0], max[1], min[2] } } ,

            { {min[0], min[1], max[2] } } ,
            { {max[0], min[1], max[2] } } ,
            { {max} },
            { {min[0], max[1], max[2] } }
        };

        return new Mesh(vertices, cubeIndices);
    }

    void bind() const;

protected:
    // TODO: we allow unnamed meshes, reconsider?
    std::unique_ptr<VertexBuffer> vb;
    std::unique_ptr<IndexBuffer> ib;
};

}
#pragma once

#include "pch.h"
#include "buffer.h"

namespace Raekor {

extern std::vector<Vertex> cubeVertices;
extern std::vector<Face> cubeIndices;
extern std::vector<Vertex> quadVertices;
extern std::vector<Face> quadIndices;

enum class Shape {
    None, Cube, Quad
};

class Mesh {
public:
    Mesh(Shape basic_shape = Shape::None);
    Mesh(std::vector<Vertex>& vb, std::vector<Face>& ib);

    void setVertexBuffer(std::vector<Vertex>& buffer);
    void setIndexBuffer(std::vector<Face>& buffer);

    const glVertexBuffer* const getVertexBuffer() const { return vb.get(); }
    const glIndexBuffer* const getIndexBuffer() const { return ib.get(); }
    
    inline void render() {
        bind();
        Renderer::DrawIndexed(ib->count);
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
    std::unique_ptr<glVertexBuffer> vb;
    std::unique_ptr<glIndexBuffer> ib;
};

}
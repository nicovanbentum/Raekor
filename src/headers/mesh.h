#pragma once

#include "pch.h"
#include "buffer.h"

namespace Raekor {

extern std::vector<Vertex> v_cube;
extern std::vector<Index> i_cube;
extern std::vector<Vertex> v_quad;
extern std::vector<Index> i_quad;

enum class Shape {
    None, Cube, Quad
};

class Mesh {
public:
    Mesh(Shape basic_shape = Shape::None);
    Mesh(const std::string& fp, const std::vector<Vertex>& vb, const std::vector<Index>& ib);

    void set_vertex_buffer(const std::vector<Vertex>& buffer);
    void set_index_buffer(const std::vector<Index>& buffer);

    const VertexBuffer* const get_vertex_buffer() const { return vb.get(); }
    const IndexBuffer* const get_index_buffer() const { return ib.get(); }
    
    inline const std::string& get_name() const { return name; }
    inline void set_name(const std::string& new_name) { name = new_name; }

    inline void render() {
        bind();
        Render::DrawIndexed(ib->get_count());
    }

    static Mesh* fromBounds(const std::array<glm::vec3, 2>& bounds) {
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

        return new Mesh("", vertices, i_cube);
    }

    void bind() const;

protected:
    // TODO: we allow unnamed meshes, reconsider?
    std::string name = "";
    std::unique_ptr<VertexBuffer> vb;
    std::unique_ptr<IndexBuffer> ib;
};

}
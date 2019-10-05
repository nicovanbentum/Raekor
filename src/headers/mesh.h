#pragma once

#include "pch.h"
#include "buffer.h"

namespace Raekor {

enum class Shape {
    None, Cube
};

class Mesh {
public:
    Mesh(Shape basic_shape = Shape::None);
    Mesh(Mesh&& rhs);

    Mesh(const Mesh& rhs) = delete;

    void set_vertex_buffer(const std::vector<Vertex>& buffer);
    void set_index_buffer(const std::vector<Index>& buffer);

    const VertexBuffer* const get_vertex_buffer() const { return vb.get(); }
    const IndexBuffer* const get_index_buffer() const { return ib.get(); }
    
    inline const std::string& get_name() const { return name; }
    inline void set_name(const std::string& new_name) { name = new_name; }

    void bind() const;

private:
    // TODO: we allow unnamed meshes, reconsider?
    std::string name = "";
    std::unique_ptr<VertexBuffer> vb;
    std::unique_ptr<IndexBuffer> ib;
};

extern std::vector<Vertex> v_cube;
extern std::vector<Index> i_cube;

}
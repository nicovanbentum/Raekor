#pragma once

#include "pch.h"
#include "buffer.h"

namespace Raekor {

class Mesh {
public:
    Mesh() {}
    Mesh(Mesh&& rhs);

    Mesh(const Mesh& rhs) = delete;

    void set_vertex_buffer(const std::vector<Vertex>& buffer);
    void set_index_buffer(const std::vector<Index>& buffer);

    const VertexBuffer* const get_vertex_buffer() const { return vb.get(); }
    const IndexBuffer* const get_index_buffer() const { return ib.get(); }
    
    inline std::string& get_name() { return name; }

    void bind() const;

private:
    std::string name;
    std::unique_ptr<VertexBuffer> vb;
    std::unique_ptr<IndexBuffer> ib;
};

// static basic shape data, very ugly pls look away

// cube without UV's
static constexpr Vertex cube[] = {
    {{-1.0f,-1.0f,-1.0f}},
    {{-1.0f,-1.0f, 1.0f}},
    {{-1.0f, 1.0f, 1.0f}},
    {{1.0f, 1.0f,-1.0f}},
    {{-1.0f,-1.0f,-1.0f}},
    {{-1.0f, 1.0f,-1.0f}},
    {{1.0f,-1.0f, 1.0f}},
    {{-1.0f,-1.0f,-1.0f}},
    {{1.0f,-1.0f,-1.0f}},
    {{1.0f, 1.0f,-1.0f}},
    {{1.0f,-1.0f,-1.0f}},
    {{-1.0f,-1.0f,-1.0f}},
    {{-1.0f,-1.0f,-1.0f}},
    {{-1.0f, 1.0f, 1.0f}},
    {{-1.0f, 1.0f,-1.0f}},
    {{1.0f,-1.0f, 1.0f}},
    {{-1.0f,-1.0f, 1.0f}},
    {{-1.0f,-1.0f,-1.0f}},
    {{-1.0f, 1.0f, 1.0f}},
    {{-1.0f,-1.0f, 1.0f}},
    {{1.0f,-1.0f, 1.0f}},
    {{1.0f, 1.0f, 1.0f}},
    {{1.0f,-1.0f,-1.0f}},
    {{1.0f, 1.0f,-1.0f}},
    {{1.0f,-1.0f,-1.0f}},
    {{1.0f, 1.0f, 1.0f}},
    {{1.0f,-1.0f, 1.0f}},
    {{1.0f, 1.0f, 1.0f}},
    {{1.0f, 1.0f,-1.0f}},
    {{-1.0f, 1.0f,-1.0f}},
    {{1.0f, 1.0f, 1.0f}},
    {{-1.0f, 1.0f,-1.0f}},
    {{-1.0f, 1.0f, 1.0f}},
    {{1.0f, 1.0f, 1.0f}},
    {{-1.0f, 1.0f, 1.0f}},
    {{1.0f,-1.0f, 1.0f}}
};

}
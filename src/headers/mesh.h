#pragma once

#include "pch.h"
#include "buffer.h"

namespace Raekor {

class Mesh {
public:
    Mesh(const std::string& filepath);

    inline std::string get_mesh_path() { return mesh_path; }
    const VertexBuffer* get_vertex_buffer() const { return vb.get(); }
    const IndexBuffer* get_index_buffer() const { return ib.get(); }

    void bind() const;
    void load_data();

private:
    std::string mesh_path;
    std::vector<Vertex> vertexes;
    std::vector<Index> indexes;
    std::unique_ptr<VertexBuffer> vb;
    std::unique_ptr<IndexBuffer> ib;
};

}
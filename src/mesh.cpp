#include "pch.h"
#include "mesh.h"
#include "util.h"
#include "renderer.h"
#include "buffer.h"

#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/Importer.hpp"

namespace Raekor {

Mesh::Mesh(Mesh&& rhs) {
    name = rhs.name;
    vb = std::move(rhs.vb);
    ib = std::move(rhs.ib);
}

void Mesh::set_vertex_buffer(const std::vector<Vertex>& buffer) {
    vb.reset(Raekor::VertexBuffer::construct(buffer));
}
void Mesh::set_index_buffer(const std::vector<Index>& buffer) {
    ib.reset(Raekor::IndexBuffer::construct(buffer));
}

void Mesh::bind() const {
    vb->bind();
    ib->bind();
}

} // Namespace Raekor
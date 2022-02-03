#pragma once

#include "buffer.h"
#include "util.h"
#include "primitives.h"

namespace Raekor {

class DXVertexBuffer {
public:
    DXVertexBuffer(const std::vector<Vertex>& vertices);
    ~DXVertexBuffer() { remove("fake.hlsl"); }
    void bind() const;
    void setLayout(const glVertexLayout& layout);

private:
    ComPtr<ID3D11Buffer> vertex_buffer;
    // this is declared mutable so we can keep set_layout as a const function
    ComPtr<ID3D11InputLayout> input_layout;
};

class DXIndexBuffer {
public:
    DXIndexBuffer(const std::vector<Triangle>& indices);
    void bind() const;

private:
    uint32_t count = 0;
    ComPtr<ID3D11Buffer> index_buffer;
};

} // namespace Raekor
#pragma once

#include "buffer.h"
#include "util.h"

namespace Raekor {

class DXVertexBuffer : public VertexBuffer {
public:
    DXVertexBuffer(const std::vector<Vertex>& vertices);
    ~DXVertexBuffer() { remove("fake.hlsl"); }
    virtual void bind() const override;
    virtual void setLayout(const InputLayout& layout) const override;

private:
    com_ptr<ID3D11Buffer> vertex_buffer;
    // this is declared mutable so we can keep set_layout as a const function
    mutable com_ptr<ID3D11InputLayout> input_layout;
};

class DXIndexBuffer : public IndexBuffer {
public:
    DXIndexBuffer(const std::vector<Triangle>& indices);
    virtual void bind() const override;

private:
    com_ptr<ID3D11Buffer> index_buffer;
};

} // namespace Raekor
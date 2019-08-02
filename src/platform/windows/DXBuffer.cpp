#include "pch.h"
#include "DXBuffer.h"
#include "DXRenderer.h"

namespace Raekor {

DXVertexBuffer::DXVertexBuffer(const std::vector<glm::vec3>& vertices) {
    // fill out the buffer description struct for our vertex buffer
    D3D11_BUFFER_DESC vb_desc = { 0 };
    vb_desc.Usage = D3D11_USAGE_DEFAULT;
    vb_desc.ByteWidth = static_cast<UINT>(sizeof(glm::vec3) * vertices.size());
    vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vb_desc.CPUAccessFlags = 0;
    vb_desc.MiscFlags = 0;

    // create the buffer that actually holds our vertices
    D3D11_SUBRESOURCE_DATA vb_data = { 0 };
    vb_data.pSysMem = &(vertices[0]);
    auto hr = D3D.device->CreateBuffer(&vb_desc, &vb_data, vertex_buffer.GetAddressOf());
    m_assert(SUCCEEDED(hr), "failed to create vertex buffer");
}

void DXVertexBuffer::bind() const {
    constexpr unsigned int stride = sizeof(glm::vec3);
    constexpr unsigned int offset = 0;
    D3D.context->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);
}

void DXVertexBuffer::unbind() const {
    constexpr unsigned int stride = sizeof(glm::vec3);
    constexpr unsigned int offset = 0;
    D3D.context->IASetVertexBuffers(0, 1, 0, &stride, &offset);
}

DXIndexBuffer::DXIndexBuffer(const std::vector<unsigned int>& indices) {
    // create our index buffer
// fill out index buffer description
    D3D11_BUFFER_DESC ib_desc = { 0 };
    ib_desc.Usage = D3D11_USAGE_DEFAULT;
    ib_desc.ByteWidth = static_cast<UINT>(sizeof(unsigned int) * indices.size());
    ib_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ib_desc.CPUAccessFlags = 0;
    ib_desc.MiscFlags = 0;

    // create a data buffer using our mesh's indices vector data
    D3D11_SUBRESOURCE_DATA ib_data;
    ib_data.pSysMem = &(indices[0]);
    auto hr = D3D.device->CreateBuffer(&ib_desc, &ib_data, index_buffer.GetAddressOf());
    m_assert(SUCCEEDED(hr), "failed to create index buffer");
}

void DXIndexBuffer::bind() const {
    D3D.context->IASetIndexBuffer(index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
}

void DXIndexBuffer::unbind() const {
    D3D.context->IASetIndexBuffer(0, DXGI_FORMAT_R32_UINT, 0);
}

} // namespace Raekor
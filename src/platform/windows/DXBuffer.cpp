#include "pch.h"
#include "DXBuffer.h"
#include "DXRenderer.h"

namespace Raekor {

std::string to_string(ShaderType type) {
    switch (type) {
    case ShaderType::FLOAT1: return "float";
    case ShaderType::FLOAT2: return "float2";
    case ShaderType::FLOAT3: return "float3";
    case ShaderType::FLOAT4: return "float4";
    default: return std::string();
    }
}

const com_ptr<ID3D10Blob> fake_shader_bytecode(const InputLayout& layout) {
    // open a filestream to a hardcoded named hlsl file 
    // We generate a struct in the shader based on the input layout
    std::ofstream file;
    file.open("fake.hlsl");
    file << "struct VS_INPUT { \n";
    char var_name = 'a';
    for (auto& element : layout) {
        auto line = std::string('\t' + to_string(element.type) + " " + var_name + " : " + element.name + ';');
        file << line << std::endl;
        var_name++;
    }
    file << "};" << std::endl << std::endl;
    file << "void main(VS_INPUT input) {}" << std::endl;
    file.close();

    // compile the fake hlsl file we just generated
    com_ptr<ID3D10Blob> shader_buffer = nullptr;
    com_ptr<ID3D10Blob> error_buffer = nullptr;
    auto hr = D3DCompileFromFile(L"fake.hlsl", NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "vs_5_0", NULL, 0, shader_buffer.GetAddressOf(), error_buffer.GetAddressOf());
    m_assert(SUCCEEDED(hr), "failed to compile fake shader");
    
    // cleanup
    remove("fake.hlsl");

    // return the buffer containing the bytecode
    return shader_buffer;
}

DXGI_FORMAT get_format(ShaderType type) {
    switch (type) {
    case ShaderType::FLOAT1: return DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT;
    case ShaderType::FLOAT2: return DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT;
    case ShaderType::FLOAT3: return DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT;
    case ShaderType::FLOAT4: return DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT;
    default: return DXGI_FORMAT_UNKNOWN;
    }
}

DXVertexBuffer::DXVertexBuffer(const std::vector<Vertex>& vertices) {
    // fill out the buffer description struct for our vertex buffer
    D3D11_BUFFER_DESC vb_desc = { 0 };
    vb_desc.Usage = D3D11_USAGE_DEFAULT;
    vb_desc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * vertices.size());
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
    constexpr unsigned int stride = sizeof(Vertex);
    constexpr unsigned int offset = 0;
    D3D.context->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);
    set_layout({ {"POSITION", ShaderType::FLOAT3}, {"TEXCOORD", ShaderType::FLOAT2} });
}

void DXVertexBuffer::set_layout(const InputLayout& layout) const {
    // if the input layout has been created we can bind it
    if (input_layout) {
        D3D.context->IASetInputLayout(input_layout.Get());
        return;
    }

    std::vector<D3D11_INPUT_ELEMENT_DESC> attributes;
    for (auto& element : layout) {
        attributes.push_back({ 
            element.name.c_str(), // name
            0, // index
            get_format(element.type), // DXGI format
            0, // input slot
            element.offset, // size per element, float3 = sizeof(float)*3
            D3D11_INPUT_PER_VERTEX_DATA, // input slot class
            0 // instance data step rate (whatever that is)
         });
    }

    auto shader = fake_shader_bytecode(layout);
    auto hr = D3D.device->CreateInputLayout(attributes.data(), (UINT)layout.size(), shader->GetBufferPointer(), shader->GetBufferSize(), input_layout.GetAddressOf());
    m_assert(SUCCEEDED(hr), "failed to create input layout");
    shader->Release();
    D3D.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    D3D.context->RSSetState(D3D.rasterize_state.Get());
    D3D.context->IASetInputLayout(input_layout.Get());

}

void DXVertexBuffer::unbind() const {
    constexpr unsigned int stride = sizeof(Vertex);
    constexpr unsigned int offset = 0;
    D3D.context->IASetVertexBuffers(0, 1, 0, &stride, &offset);
}

DXIndexBuffer::DXIndexBuffer(const std::vector<Index>& indices) {
    count = (unsigned int)(indices.size() * 3);
    // create our index buffer
    // fill out index buffer description
    D3D11_BUFFER_DESC ib_desc = { 0 };
    ib_desc.Usage = D3D11_USAGE_DEFAULT;
    ib_desc.ByteWidth = static_cast<UINT>(sizeof(Index) * indices.size());
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
#include "pch.h"
#include "DXBuffer.h"
#include "DXRenderer.h"
#include "primitives.h"

namespace Raekor {

// TODO: Add support for most HLSL types
std::string to_string(ShaderType type) {
    switch (type) {
    case ShaderType::FLOAT: return "float";
    case ShaderType::FLOAT2: return "float2";
    case ShaderType::FLOAT3: return "float3";
    case ShaderType::FLOAT4: return "float4";
    default: return std::string();
    }
}

// Filthy hack because I don't feel like implementing an entire pipeline using PSO's right now
ID3D10Blob* fake_shader_bytecode(const glVertexLayout& layout) {
    // open a filestream to a hardcoded named hlsl file 
    // We generate a struct in the shader based on the input layout
    {
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
    }

    // compile the fake hlsl file we just generated
    ID3D10Blob* shader_buffer = nullptr;
    ID3D10Blob* error_buffer = nullptr;
    auto hr = D3DCompileFromFile(L"fake.hlsl", NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "vs_5_0", NULL, 0, &shader_buffer, &error_buffer);
    m_assert(SUCCEEDED(hr), "failed to compile fake shader");
#if 0
    if (FAILED(hr) && error_buffer) {
        const char* msg = (const char*)error_buffer->GetBufferPointer();
        std::cout << msg << std::endl;
    }
#endif
    // return the buffer containing the bytecode
    return shader_buffer;
}

// TODO: Add support for most HLSL types
DXGI_FORMAT get_format(ShaderType type) {
    switch (type) {
    case ShaderType::FLOAT: return DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT;
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

// TODO: remove this set_layout call, setting the layout is a user-side operation
void DXVertexBuffer::bind() const {
    constexpr unsigned int stride = sizeof(Vertex);
    constexpr unsigned int offset = 0;
    D3D.context->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);
    D3D.context->IASetInputLayout(input_layout.Get());
}

void DXVertexBuffer::setLayout(const glVertexLayout& layout) {
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

    ID3D10Blob* shader = fake_shader_bytecode(layout);
    auto hr = D3D.device->CreateInputLayout(attributes.data(), (UINT)layout.size(), shader->GetBufferPointer(), shader->GetBufferSize(), input_layout.GetAddressOf());
    m_assert(SUCCEEDED(hr), "failed to create input layout");
    shader->Release();
}

DXIndexBuffer::DXIndexBuffer(const std::vector<Triangle>& indices) {
    count = (unsigned int)(indices.size() * 3);
    // create our index buffer
    // fill out index buffer description
    D3D11_BUFFER_DESC ib_desc = { 0 };
    ib_desc.Usage = D3D11_USAGE_DEFAULT;
    ib_desc.ByteWidth = static_cast<UINT>(sizeof(Triangle) * indices.size());
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

} // namespace Raekor
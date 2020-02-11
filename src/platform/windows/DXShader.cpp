#include "pch.h"
#include "DXShader.h"
#include "DXRenderer.h"

namespace Raekor {

DXShader::DXShader(Stage* stages, size_t stageCount) {
    for (unsigned int i = 0; i < stageCount; i++) {
        Stage& stage = stages[i];
        auto fp = std::string(stage.filepath);
        auto ww = std::wstring(fp.begin(), fp.end());
        LPCWSTR wstr = ww.c_str();

        com_ptr<ID3D10Blob> buffer;
        auto hr = D3DReadFileToBlob(wstr, buffer.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("failed to read DirectX file to blob");


        switch (stage.type) {
            case Type::VERTEX: {
                hr = D3D.device->CreateVertexShader(buffer.Get()->GetBufferPointer(), buffer->GetBufferSize(), NULL, vertex_shader.GetAddressOf());
            } break;
            case Type::FRAG: {
                hr = D3D.device->CreatePixelShader(buffer.Get()->GetBufferPointer(), buffer->GetBufferSize(), NULL, pixel_shader.GetAddressOf());
            } break;
            case Type::GEO: {
                hr = D3D.device->CreateGeometryShader(buffer.Get()->GetBufferPointer(), buffer->GetBufferSize(), NULL, geo_shader.GetAddressOf());
            } break;
            case Type::COMPUTE: {
                hr = D3D.device->CreateComputeShader(buffer.Get()->GetBufferPointer(), buffer->GetBufferSize(), NULL, compute_shader.GetAddressOf());
            } break;
        }
        if (FAILED(hr)) throw std::runtime_error("failed to create DX shader");
    }
}

const void DXShader::bind() const {
    if (vertex_shader)
        D3D.context->VSSetShader(vertex_shader.Get(), NULL, 0);
    if (pixel_shader)
        D3D.context->PSSetShader(pixel_shader.Get(), NULL, 0);
    if (geo_shader)
        D3D.context->GSSetShader(geo_shader.Get(), NULL, 0);
    if (compute_shader)
        D3D.context->CSSetShader(compute_shader.Get(), NULL, 0);
}

const void DXShader::unbind() const {
    D3D.context->VSSetShader(NULL, NULL, 0);
    D3D.context->PSSetShader(NULL, NULL, 0);
    D3D.context->GSSetShader(NULL, NULL, 0);
    D3D.context->CSSetShader(NULL, NULL, 0);
}



} // namespace Raekor
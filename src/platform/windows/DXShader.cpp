#include "pch.h"
#include "DXShader.h"
#include "DXRenderer.h"

namespace Raekor {

DXShader::DXShader(std::string vertex, std::string pixel) {
    auto vw = std::wstring(vertex.begin(), vertex.end());
    auto pw = std::wstring(pixel.begin(), pixel.end());
    LPCWSTR vertex_wstr = vw.c_str();
    LPCWSTR pixel_wstr = pw.c_str();


    auto hr = D3DReadFileToBlob(vertex_wstr, vertex_shader_buffer.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("failed to read DirectX file to blob");
    hr = D3DReadFileToBlob(pixel_wstr, pixel_shader_buffer.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("failed to read DirectX file to blob");
    hr = D3D.device->CreateVertexShader(vertex_shader_buffer.Get()->GetBufferPointer(), vertex_shader_buffer->GetBufferSize(), NULL, vertex_shader.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("failed to create DirectX vertex shader");
    hr = D3D.device->CreatePixelShader(pixel_shader_buffer.Get()->GetBufferPointer(), pixel_shader_buffer->GetBufferSize(), NULL, pixel_shader.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("failed to create DX pixel shader");
}

const void DXShader::bind() const {
    D3D.context->VSSetShader(vertex_shader.Get(), NULL, 0);
    D3D.context->PSSetShader(pixel_shader.Get(), NULL, 0);
}

const void DXShader::unbind() const {
    D3D.context->VSSetShader(NULL, NULL, 0);
    D3D.context->PSSetShader(NULL, NULL, 0);
}



} // namespace Raekor
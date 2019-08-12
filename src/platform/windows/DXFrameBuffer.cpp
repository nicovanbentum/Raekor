#include "pch.h"
#include "DXFrameBuffer.h"
#include "DXRenderer.h"

namespace Raekor {

DXFrameBuffer::DXFrameBuffer(const glm::vec2& size) {
    this->size = size;

    D3D11_TEXTURE2D_DESC texture_desc = { 0 };
    D3D11_RENDER_TARGET_VIEW_DESC target_desc;
    D3D11_SHADER_RESOURCE_VIEW_DESC res_desc;

    // set the directx viewport
    view_port = { 0 };
    view_port.TopLeftX = 0;
    view_port.TopLeftY = 0;
    view_port.Width = size.x;
    view_port.Height = size.y;
    view_port.MinDepth = 0.0f;
    view_port.MaxDepth = 1.0f;

    //fill out texture description
    texture_desc.Width = (UINT)size.x;
    texture_desc.Height = (UINT)size.y;
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = 1;
    texture_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texture_desc.CPUAccessFlags = 0;
    texture_desc.MiscFlags = 0;

    auto hr = D3D.device->CreateTexture2D(&texture_desc, NULL, texture.GetAddressOf());
    m_assert(SUCCEEDED(hr), "failed to create 2d texture");

    target_desc.Format = texture_desc.Format;
    target_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    target_desc.Texture2D.MipSlice = 0;

    hr = D3D.device->CreateRenderTargetView(texture.Get(), &target_desc, target_view.GetAddressOf());
    m_assert(SUCCEEDED(hr), "failed to create render target view");

    res_desc.Format = texture_desc.Format;
    res_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    res_desc.Texture2D.MostDetailedMip = 0;
    res_desc.Texture2D.MipLevels = 1;

    hr = D3D.device->CreateShaderResourceView(texture.Get(), &res_desc, shader_view.GetAddressOf());
}

DXFrameBuffer::~DXFrameBuffer() {}

void DXFrameBuffer::bind() const {
    D3D.context->RSSetViewports(1, &view_port);
    D3D.render_target_view = target_view;
    D3D.context->OMSetRenderTargets(1, target_view.GetAddressOf(), D3D.depth_stencil_view.Get());
}

void DXFrameBuffer::unbind() const {
    D3D.render_target_view = D3D.back_buffer;
    D3D.context->OMSetRenderTargets(1, D3D.back_buffer.GetAddressOf(), NULL);
}

void* DXFrameBuffer::ImGui_data() const {
    return (void*)(shader_view.Get());
}

// TODO: figure this out
void DXFrameBuffer::resize(const glm::vec2& size) {

}


} // Raekor
#include "pch.h"
#include "DXTexture.h"
#include "DXRenderer.h"

namespace Raekor {

DXTexture::DXTexture(const std::string& filepath) {
    auto hr = DirectX::CreateWICTextureFromFile(D3D.device.Get(), L"resources\\textures\\grass block.png", nullptr, texture.GetAddressOf());
    m_assert(SUCCEEDED(hr), "failed to load texture");

    D3D11_SAMPLER_DESC samp_desc;
    memset(&samp_desc, 0, sizeof(D3D11_SAMPLER_DESC));
    samp_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samp_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samp_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samp_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samp_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samp_desc.MinLOD = 0;
    samp_desc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = D3D.device->CreateSamplerState(&samp_desc, sampler_state.GetAddressOf());
    m_assert(SUCCEEDED(hr), "failed to crreate sampler state");
}

void DXTexture::bind() const {
    D3D.context->PSSetShaderResources(0, 1, texture.GetAddressOf());
    D3D.context->PSSetSamplers(0, 1, sampler_state.GetAddressOf());
}

} // namespace Raekor
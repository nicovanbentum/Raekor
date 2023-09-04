#include "pch.h"
#include "DXTexture.h"
#include "DXRenderer.h"
#include "Raekor/timer.h"

namespace Raekor {

// TODO: right now we always let directx auto generate mip maps, might want to make that optional
DXTexture::DXTexture(const std::string& filepath)
{
    stbi_set_flip_vertically_on_load(true);

    //describe our 2d texture
    D3D11_TEXTURE2D_DESC desc = {};
    desc.MipLevels = 0;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    // load the image data using stb image
    D3D11_SUBRESOURCE_DATA image_data;
    int width, height, channels;
    Timer timer;
    auto image = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    assert(image && "failed to load directx texture using stbi");
    std::cout << "DirectX stb texture load time : " << Timer::sToMilliseconds(timer.GetElapsedTime()) << " ms" << '\n';

    // point the directx resource to the stb image data
    image_data.pSysMem = (const void*)image;
    image_data.SysMemPitch = STBI_rgb_alpha * width;
    desc.Width = width;
    desc.Height = height;

    auto hr = D3D.device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf());
    assert(SUCCEEDED(hr) && "failed to create 2d texture object");

    // describe the shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC resource = {};
    resource.Format = desc.Format;
    resource.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    resource.TextureCube.MipLevels = (UINT)-1; // intentional overflow? microsoft please..

    hr = D3D.device->CreateShaderResourceView(texture.Get(), &resource, &texture_resource);
    assert(SUCCEEDED(hr) && "failed to create shader resource view for dx texture");

    D3D.context->UpdateSubresource(texture.Get(), 0, 0, (const void*)image, image_data.SysMemPitch, 0);
    D3D.context->GenerateMips(texture_resource.Get());

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
    m_assert(SUCCEEDED(hr), "failed to create sampler state");
}

DXTexture::DXTexture(uint32_t width, uint32_t height, const void* pixels)
{
    //describe our 2d texture
    D3D11_TEXTURE2D_DESC desc = {};
    desc.MipLevels = 0;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;


    // point the directx resource to the stb image data
    D3D11_SUBRESOURCE_DATA image_data;
    image_data.pSysMem = (const void*)pixels;
    image_data.SysMemPitch = STBI_rgb_alpha * width;
    desc.Width = width;
    desc.Height = height;

    auto hr = D3D.device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf());
    m_assert(SUCCEEDED(hr), "failed to create 2d texture object");

    // describe the shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC resource = {};
    resource.Format = desc.Format;
    resource.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    resource.TextureCube.MipLevels = (UINT)-1;

    hr = D3D.device->CreateShaderResourceView(texture.Get(), &resource, &texture_resource);
    m_assert(SUCCEEDED(hr), "failed to create shader resource view for dx texture");

    D3D.context->UpdateSubresource(texture.Get(), 0, 0, (const void*)pixels, image_data.SysMemPitch, 0);
    D3D.context->GenerateMips(texture_resource.Get());

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
    m_assert(SUCCEEDED(hr), "failed to create sampler state");
}

void DXTexture::bind(uint32_t slot) const
{
    D3D.context->PSSetShaderResources(0, 1, texture_resource.GetAddressOf());
    D3D.context->PSSetSamplers(0, 1, sampler_state.GetAddressOf());
}

DXTextureCube::DXTextureCube(const std::array<std::string, 6>& face_files)
{
    stbi_set_flip_vertically_on_load(false);
    D3D11_TEXTURE2D_DESC desc = {};
    desc.MipLevels = 1;
    desc.ArraySize = 6;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    D3D11_SUBRESOURCE_DATA data[6];
    stbi_uc* images[6];

    // for every face file we generate an OpenGL texture image
    int width = -1, height = -1, n_channels = -1;
    for (unsigned int i = 0; i < face_files.size(); i++)
    {
        auto image = stbi_load(face_files[i].c_str(), &width, &height, &n_channels, STBI_rgb_alpha);
        images[i] = image;
        if (!image) std::cout << "failed to load stbi image fifle" << std::endl;
        data[i].pSysMem = (const void*)image;
        data[i].SysMemPitch = STBI_rgb_alpha * width;
    }

    desc.Width = width;
    desc.Height = height;

    D3D11_SHADER_RESOURCE_VIEW_DESC resource;
    resource.Format = desc.Format;
    resource.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    resource.TextureCube.MipLevels = desc.MipLevels;
    resource.TextureCube.MostDetailedMip = 0;


    auto hr = D3D.device->CreateTexture2D(&desc, &data[0], texture.GetAddressOf());
    m_assert(SUCCEEDED(hr), "failed to create 2d texture object for cubemap");

    hr = D3D.device->CreateShaderResourceView(texture.Get(), &resource, &texture_resource);
    m_assert(SUCCEEDED(hr), "failed to create shader resource view for cubemap");

    for (unsigned int i = 0; i < ARRAYSIZE(images); i++)
    {
        stbi_image_free(images[i]);
    }
}

void DXTextureCube::bind(uint32_t slot) const
{
    D3D.context->PSSetShaderResources(0, 1, texture_resource.GetAddressOf());
}



} // namespace Raekor
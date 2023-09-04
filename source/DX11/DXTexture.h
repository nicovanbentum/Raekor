#pragma once

namespace Raekor {

class DXTexture
{
public:
    DXTexture(const std::string& filepath);
    DXTexture(uint32_t width, uint32_t height, const void* pixels);
    virtual void bind(uint32_t slot) const;

private:
    ComPtr<ID3D11SamplerState> sampler_state;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> texture_resource;
};

class DXTextureCube
{
public:
    DXTextureCube(const std::array<std::string, 6>& face_files);
    virtual void bind(uint32_t slot) const;

private:
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> texture_resource;
};

} // namespace Raekor
#pragma once

#include "util.h"
#include "texture.h"

namespace Raekor {

class DXTexture {
public:
    DXTexture(const std::string& filepath);
    DXTexture(const Stb::Image& image);
    virtual void bind(uint32_t slot) const;

private:
    com_ptr<ID3D11SamplerState> sampler_state;
    com_ptr<ID3D11Texture2D> texture;
    com_ptr<ID3D11ShaderResourceView> texture_resource;
};

class DXTextureCube {
public:
    DXTextureCube(const std::array<std::string, 6>& face_files);
    virtual void bind(uint32_t slot) const;

private:
    com_ptr<ID3D11Texture2D> texture;
    com_ptr<ID3D11ShaderResourceView> texture_resource;
};

} // namespace Raekor
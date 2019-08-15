#pragma once

#include "util.h"
#include "texture.h"

namespace Raekor {

class DXTexture : public Texture {
public:
    DXTexture(const std::string& filepath);
    virtual void bind() const override;

private:
    com_ptr<ID3D11SamplerState> sampler_state;
    com_ptr<ID3D11Texture2D> texture;
    com_ptr<ID3D11ShaderResourceView> texture_resource;
};

class DXTextureCube : public Texture {
public:
    DXTextureCube(const std::array<std::string, 6>& face_files);
    virtual void bind() const override;

private:
    com_ptr<ID3D11Texture2D> texture;
    com_ptr<ID3D11ShaderResourceView> texture_resource;
};

} // namespace Raekor
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
    com_ptr<ID3D11ShaderResourceView> texture;
};

} // namespace Raekor
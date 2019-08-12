#pragma once

#include "util.h"
#include "framebuffer.h"

namespace Raekor {

class DXFrameBuffer : public FrameBuffer {
public:
    DXFrameBuffer(const glm::vec2& size);
    ~DXFrameBuffer();
    virtual void bind() const override;
    virtual void unbind() const override;
    virtual void* ImGui_data() const override;
    virtual void resize(const glm::vec2& size) override;

private:
    D3D11_VIEWPORT view_port;
    com_ptr<ID3D11Texture2D> texture;
    com_ptr<ID3D11RenderTargetView> target_view;
    com_ptr<ID3D11ShaderResourceView> shader_view;
};

} // Raekor
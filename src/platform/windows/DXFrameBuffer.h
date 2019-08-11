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
    ID3D11ShaderResourceView* get_data() { return shader_view.Get(); }

private:
    com_ptr<ID3D11Texture2D> texture;
    com_ptr<ID3D11RenderTargetView> target_view;
    com_ptr<ID3D11ShaderResourceView> shader_view;
};

} // Raekor
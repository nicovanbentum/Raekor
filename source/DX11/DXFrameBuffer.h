#pragma once

#include "pch.h"

namespace Raekor {

class DXFrameBuffer
{
public:
    struct ConstructInfo
    {
        glm::vec2 size;
        // opt
        bool depthOnly = false;
        bool writeOnly = false;
        bool HDR = false;
    };

    DXFrameBuffer(ConstructInfo* info);
    ~DXFrameBuffer();

    void bind() const;
    void unbind() const;

    void Image() const;

    void resize(const glm::vec2& size);
    glm::vec2 getSize() const { return size; }

private:
    glm::vec2 size;
    D3D11_VIEWPORT view_port;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> target_view;
    ComPtr<ID3D11ShaderResourceView> shader_view;
};

} // Raekor
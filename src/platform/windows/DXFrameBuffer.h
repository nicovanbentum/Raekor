#pragma once

#include "util.h"

namespace Raekor {

/*
    old framebuffer api, should be removed
*/
class FrameBuffer {
public:
    struct ConstructInfo {
        glm::vec2 size;
        // opt
        bool depthOnly = false;
        bool writeOnly = false;
        bool HDR = false;
    };

public:
    virtual ~FrameBuffer() {}
    static FrameBuffer* construct(ConstructInfo* info);
    virtual void bind() const = 0;
    virtual void unbind() const = 0;
    virtual void Image() const = 0;
    virtual void resize(const glm::vec2& size) = 0;
    inline glm::vec2 getSize() const { return size; }

protected:
    glm::vec2 size;
    unsigned int fboID;
    unsigned int rboID;
    unsigned int textureID;
};

class DXFrameBuffer : public FrameBuffer {
public:
    DXFrameBuffer(FrameBuffer::ConstructInfo* info);
    ~DXFrameBuffer();
    virtual void bind() const override;
    virtual void unbind() const override;

    virtual void Image() const override;
    virtual void resize(const glm::vec2& size) override;

private:
    D3D11_VIEWPORT view_port;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> target_view;
    ComPtr<ID3D11ShaderResourceView> shader_view;
};

} // Raekor
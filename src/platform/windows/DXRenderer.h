#pragma once

#include "pch.h"
#include "renderer.h"

namespace Raekor {
// TODO: This should probably be moved to the DXRenderer class as static members
struct COM_PTRS {
    ComPtr<IDXGISwapChain> swap_chain;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11RenderTargetView> back_buffer;
    ComPtr<ID3D11RasterizerState> rasterize_state;
    ComPtr<ID3D11DepthStencilView> depth_stencil_view;
    ComPtr<ID3D11RenderTargetView> render_target_view;

    void Release() {
        swap_chain->Release();
        device->Release();
        context->Release();
        back_buffer->Release();
        rasterize_state->Release();
        depth_stencil_view->Release();
        render_target_view->Release();
    }
};

extern COM_PTRS D3D;

class DXRenderer : public Renderer {
public:
    DXRenderer(SDL_Window* window);
    ~DXRenderer();
    virtual void impl_ImGui_Render()                                     override;
    virtual void impl_ImGui_NewFrame(SDL_Window* window)                 override;
    virtual void impl_Clear(glm::vec4 color)                             override;
    virtual void impl_DrawIndexed(unsigned int size)                     override;
    virtual void impl_SwapBuffers(bool vsync) const                      override;

private:
    ComPtr<ID3D11Texture2D> depth_stencil_buffer;
    ComPtr<ID3D11DepthStencilState> depth_stencil_state;
    ComPtr<ID3D11BlendState> blend_state;
};

} // namespace Raekor
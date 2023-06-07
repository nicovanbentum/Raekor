#pragma once

#include "pch.h"
#include "DXShader.h"

namespace Raekor {

inline void gThrowIfFailed(HRESULT inResult) {
    if (FAILED(inResult))
        __debugbreak();
}

// TODO: This should probably be moved to the DXRenderer class as static members
struct COM_PTRS {
    ComPtr<IDXGISwapChain> swap_chain = nullptr;
    ComPtr<ID3D11Device> device = nullptr;
    ComPtr<ID3D11DeviceContext> context = nullptr;
    ComPtr<ID3D11RenderTargetView> back_buffer = nullptr;
    ComPtr<ID3D11RasterizerState> rasterize_state = nullptr;
    ComPtr<ID3D11DepthStencilView> depth_stencil_view = nullptr;
    ComPtr<ID3D11RenderTargetView> render_target_view = nullptr;

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

class DXRenderer {
public:
    DXRenderer(const Viewport& inViewport, SDL_Window* window);
    ~DXRenderer();

    void ImGui_Render();
    void ImGui_NewFrame(SDL_Window* window);
    void Clear(glm::vec4 color);
    void BindPipeline(D3D11_COMPARISON_FUNC inDepthFunc = D3D11_COMPARISON_LESS);
    void SwapBuffers(bool vsync) const;

private:
    SDL_Window* renderWindow;
    ComPtr<ID3D11Texture2D> depth_stencil_buffer;
    ComPtr<ID3D11DepthStencilState> depth_stencil_state;
    ComPtr<ID3D11DepthStencilState> depth_stencil_state_equal;
    ComPtr<ID3D11BlendState> blend_state;
};

} // namespace Raekor
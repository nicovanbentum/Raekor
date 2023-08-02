#include "pch.h"
#include "DXRenderer.h"
#include "Raekor/gui.h"

#define DEPTH_BIAS_D32_FLOAT(d) (d/(1/pow(2,23)))

namespace Raekor {

// TODO: globals are bad mkay
COM_PTRS D3D;

DXRenderer::DXRenderer(const Viewport& inViewport, SDL_Window* window) {
    std::cout << "Active Rendering API: DirectX11  No device/context information available.\n";

    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().IniFilename = "";
    GUI::SetTheme();

    renderWindow = window;
    auto hr = CoInitialize(NULL);
    assert(SUCCEEDED(hr) && "failed to initialize microsoft WIC");

    // query SDL's machine info for the window's HWND
    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version);
    SDL_GetWindowWMInfo(window, &wminfo);
    auto dx_hwnd = wminfo.info.win.window;

    auto device_flags = NULL;
#ifndef NDEBUG
    device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    DXGI_SWAP_CHAIN_DESC sc_desc = {};
    sc_desc.Windowed          = TRUE;
    sc_desc.BufferCount       = 1u;
    sc_desc.OutputWindow      = dx_hwnd;
    sc_desc.SampleDesc.Count  = 1u;
    sc_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sc_desc.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;

    hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, device_flags,
        NULL, NULL, D3D11_SDK_VERSION, &sc_desc, D3D.swap_chain.GetAddressOf(), D3D.device.GetAddressOf(), NULL, D3D.context.GetAddressOf());
    assert(!FAILED(hr) && "failed to init device and swap chain");

#ifndef NDEBUG
    ComPtr<ID3D11InfoQueue> info_queue;
    D3D.device->QueryInterface(IID_PPV_ARGS(info_queue.GetAddressOf()));
    info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
    info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, TRUE);
#endif

    ComPtr<ID3D11Texture2D> backbuffer;
    D3D.swap_chain->GetBuffer(0, IID_PPV_ARGS(backbuffer.GetAddressOf()));
    gThrowIfFailed(D3D.device->CreateRenderTargetView(backbuffer.Get(), NULL, D3D.back_buffer.GetAddressOf()));

    const auto viewport = CD3D11_VIEWPORT(0.0f, 0.0f, (FLOAT)inViewport.GetSize().x, (FLOAT)inViewport.GetSize().y);
    D3D.context->RSSetViewports(1, &viewport);

    auto blend_desc = CD3D11_BLEND_DESC(CD3D11_DEFAULT());
    D3D.device->CreateBlendState(&blend_desc, &blend_state);

    auto depth_target_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_D32_FLOAT, inViewport.GetSize().x, inViewport.GetSize().y, 1u, 1u, D3D11_BIND_DEPTH_STENCIL);
    gThrowIfFailed(D3D.device->CreateTexture2D(&depth_target_desc, NULL, depth_stencil_buffer.GetAddressOf()));

    gThrowIfFailed(D3D.device->CreateDepthStencilView(depth_stencil_buffer.Get(), NULL, D3D.depth_stencil_view.GetAddressOf()));

    D3D.context->OMSetRenderTargets(1, D3D.back_buffer.GetAddressOf(), D3D.depth_stencil_view.Get());

    auto depth_stencil_desc = CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT());
    gThrowIfFailed(D3D.device->CreateDepthStencilState(&depth_stencil_desc, depth_stencil_state.GetAddressOf()));

    depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    gThrowIfFailed(D3D.device->CreateDepthStencilState(&depth_stencil_desc, depth_stencil_state_equal.GetAddressOf()));

    // fill out the raster description struct and create a rasterizer state
    auto raster_desc = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
    raster_desc.FrontCounterClockwise = TRUE;
    gThrowIfFailed(D3D.device->CreateRasterizerState(&raster_desc, D3D.rasterize_state.GetAddressOf()));

    D3D.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    D3D.context->RSSetState(D3D.rasterize_state.Get());

    // initialize ImGui for DirectX
    ImGui_ImplSDL2_InitForD3D(window);
    ImGui_ImplDX11_Init(D3D.device.Get(), D3D.context.Get());

    D3D.render_target_view = D3D.back_buffer;
}

DXRenderer::~DXRenderer() {
    ImGui_ImplDX11_InvalidateDeviceObjects();
}

void DXRenderer::Clear(glm::vec4 color) {
    const float clear_color[4] = { color.r, color.g, color.b, color.a };
    D3D.context->ClearDepthStencilView(D3D.depth_stencil_view.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    D3D.context->ClearRenderTargetView(D3D.render_target_view.Get(), clear_color);
}

void DXRenderer::ImGui_NewFrame(SDL_Window* window) {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();
}

void DXRenderer::ImGui_Render() {
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void DXRenderer::BindPipeline(D3D11_COMPARISON_FUNC inDepthFunc) {
    if (inDepthFunc == D3D11_COMPARISON_LESS)
        D3D.context->OMSetDepthStencilState(depth_stencil_state.Get(), 0);
    else if (inDepthFunc == D3D11_COMPARISON_EQUAL)
        D3D.context->OMSetDepthStencilState(depth_stencil_state_equal.Get(), 0);

    float blendFactor[] = { 0, 0, 0, 0 };
    UINT sampleMask = 0xffffffff;
    D3D.context->OMSetBlendState(blend_state.Get(), blendFactor, sampleMask);
}

void DXRenderer::SwapBuffers(bool vsync) const {
    D3D.swap_chain->Present(vsync, NULL);
}

} // namespace Raekor

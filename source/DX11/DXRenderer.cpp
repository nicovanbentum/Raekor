#include "pch.h"
#include "DXRenderer.h"

namespace Raekor {

// TODO: globals are bad mkay
COM_PTRS D3D;

DXRenderer::DXRenderer(SDL_Window* window) {
    std::cout << "Active Rendering API: DirectX11  No device/context information available." << std::endl;

    renderWindow = window;
    auto hr = CoInitialize(NULL);
    m_assert(SUCCEEDED(hr), "failed to initialize microsoft WIC");

    // query SDL's machine info for the window's HWND
    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version);
    SDL_GetWindowWMInfo(window, &wminfo);
    auto dx_hwnd = wminfo.info.win.window;

    auto device_flags = NULL;
#ifndef NDBUG
    //device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // fill out the swap chain description and create both the device and swap chain
    DXGI_SWAP_CHAIN_DESC sc_desc = { 0 };
    sc_desc.BufferCount = 1;
    sc_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sc_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc_desc.OutputWindow = dx_hwnd;
    sc_desc.SampleDesc.Count = 4;
    sc_desc.Windowed = TRUE;
    hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, device_flags,
        NULL, NULL, D3D11_SDK_VERSION, &sc_desc, D3D.swap_chain.GetAddressOf(), D3D.device.GetAddressOf(), NULL, D3D.context.GetAddressOf());
    m_assert(!FAILED(hr), "failed to init device and swap chain");

    // setup the backbuffer
    ID3D11Texture2D* backbuffer_addr;
    D3D.swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backbuffer_addr);
    D3D.device->CreateRenderTargetView(backbuffer_addr, NULL, D3D.back_buffer.GetAddressOf());
    backbuffer_addr->Release();
    D3D.context->OMSetRenderTargets(1, D3D.back_buffer.GetAddressOf(), NULL);

    int ww, wh;
    SDL_GetWindowSize(window, &ww, &wh);

    // set the directx viewport
    D3D11_VIEWPORT viewport = { 0 };
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = (FLOAT)ww;
    viewport.Height = (FLOAT)wh;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    D3D.context->RSSetViewports(1, &viewport);

    // setup the depth stencil buffer for depth testing
    D3D11_TEXTURE2D_DESC dstexdesc;
    dstexdesc.Width = ww;
    dstexdesc.Height = wh;
    dstexdesc.MipLevels = 1;
    dstexdesc.ArraySize = 1;
    dstexdesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dstexdesc.SampleDesc.Count = 1;
    dstexdesc.SampleDesc.Quality = 0;
    dstexdesc.Usage = D3D11_USAGE_DEFAULT;
    dstexdesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    dstexdesc.CPUAccessFlags = 0;
    dstexdesc.MiscFlags = 0;

    D3D11_BLEND_DESC bsd = {};
    bsd.RenderTarget[0].BlendEnable = TRUE;
    bsd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    bsd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;

    bsd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bsd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
    bsd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    bsd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bsd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    D3D.device->CreateBlendState(&bsd, &blend_state);

    hr = D3D.device->CreateTexture2D(&dstexdesc, NULL, depth_stencil_buffer.GetAddressOf());
    m_assert(SUCCEEDED(hr), "failed to create 2d stencil depth texture");

    hr = D3D.device->CreateDepthStencilView(depth_stencil_buffer.Get(), NULL, D3D.depth_stencil_view.GetAddressOf());
    m_assert(SUCCEEDED(hr), "failed to create depth stencil view");

    D3D11_DEPTH_STENCIL_DESC dsdesc;
    memset(&dsdesc, 0, sizeof(D3D11_DEPTH_STENCIL_DESC));
    dsdesc.DepthEnable = true;
    dsdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK::D3D11_DEPTH_WRITE_MASK_ALL;
    dsdesc.DepthFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_LESS_EQUAL;

    dsdesc.StencilEnable = true;
    dsdesc.StencilReadMask = 0xFF;
    dsdesc.StencilWriteMask = 0xFF;

    // Stencil operations if pixel is front-facing.
    dsdesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    dsdesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
    dsdesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    dsdesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    // Stencil operations if pixel is back-facing.
    dsdesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    dsdesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
    dsdesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    dsdesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    hr = D3D.device->CreateDepthStencilState(&dsdesc, depth_stencil_state.GetAddressOf());
    m_assert(SUCCEEDED(hr), "failed to create depth stencil state");

    // fill out the raster description struct and create a rasterizer state
    D3D11_RASTERIZER_DESC raster_desc;
    memset(&raster_desc, 0, sizeof(D3D11_RASTERIZER_DESC));
    raster_desc.FillMode = D3D11_FILL_MODE::D3D11_FILL_SOLID;
    raster_desc.CullMode = D3D11_CULL_MODE::D3D11_CULL_NONE;

    hr = D3D.device->CreateRasterizerState(&raster_desc, D3D.rasterize_state.GetAddressOf());
    m_assert(SUCCEEDED(hr), "failed to create rasterizer state");
    D3D.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    D3D.context->RSSetState(D3D.rasterize_state.Get());

    // initialize ImGui for DirectX
    ImGui_ImplSDL2_InitForD3D(window);
    ImGui_ImplDX11_Init(D3D.device.Get(), D3D.context.Get());

    D3D.render_target_view = D3D.back_buffer;
}

DXRenderer::~DXRenderer() {
    ImGui_ImplDX11_InvalidateDeviceObjects();
    D3D.Release();
}

void DXRenderer::impl_Clear(glm::vec4 color) {
    const float clear_color[4] = { color.r, color.g, color.b, color.a };
    D3D.context->ClearDepthStencilView(D3D.depth_stencil_view.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    D3D.context->ClearRenderTargetView(D3D.render_target_view.Get(), clear_color);
}

void DXRenderer::impl_ImGui_NewFrame(SDL_Window* window) {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();
}

void DXRenderer::impl_ImGui_Render() {
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void DXRenderer::impl_DrawIndexed(unsigned int size) {
    D3D.context->OMSetDepthStencilState(depth_stencil_state.Get(), 0);
    float blendFactor[] = { 0, 0, 0, 0 };
    UINT sampleMask = 0xffffffff;
    D3D.context->OMSetBlendState(blend_state.Get(), blendFactor, sampleMask);
    D3D.context->DrawIndexed(size, 0, 0);
}

void DXRenderer::impl_SwapBuffers(bool vsync) const {
    D3D.swap_chain->Present(vsync, NULL);
}

} // namespace Raekor

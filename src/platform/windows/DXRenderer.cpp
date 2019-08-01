#include "pch.h"
#include "DXRenderer.h"

namespace Raekor {

// TODO: globals are bad mkay
COM_PTRS D3D;

DXRenderer::DXRenderer(SDL_Window* window) {
	SDL_SysWMinfo wminfo;
	SDL_VERSION(&wminfo.version);
	SDL_GetWindowWMInfo(window, &wminfo);
	auto dx_hwnd = wminfo.info.win.window;

	// fill out the swap chain description and create both the device and swap chain
	DXGI_SWAP_CHAIN_DESC sc_desc = { 0 };
	sc_desc.BufferCount = 1;
	sc_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sc_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sc_desc.OutputWindow = dx_hwnd;
	sc_desc.SampleDesc.Count = 4;
	sc_desc.Windowed = TRUE;
	auto hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, NULL,
		NULL, NULL, D3D11_SDK_VERSION, &sc_desc, D3D.swap_chain.GetAddressOf(), D3D.device.GetAddressOf(), NULL, D3D.context.GetAddressOf());
	m_assert(!FAILED(hr), "failed to init device and swap chain");

	ID3D11Texture2D* backbuffer_addr;
	D3D.swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backbuffer_addr);
	D3D.device->CreateRenderTargetView(backbuffer_addr, NULL, D3D.back_buffer.GetAddressOf());
	backbuffer_addr->Release();
	D3D.context->OMSetRenderTargets(1, D3D.back_buffer.GetAddressOf(), NULL);

	// set the directx viewport
	D3D11_VIEWPORT viewport = { 0 };
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = 1280;
	viewport.Height = 720;
	D3D.context->RSSetViewports(1, &viewport);

	// fill out the raster description struct and create a rasterizer state
	D3D11_RASTERIZER_DESC raster_desc = { 0 };
	raster_desc.FillMode = D3D11_FILL_MODE::D3D11_FILL_SOLID;
	raster_desc.CullMode = D3D11_CULL_MODE::D3D11_CULL_NONE;
	raster_desc.FrontCounterClockwise = false;
	hr = D3D.device->CreateRasterizerState(&raster_desc, D3D.rasterize_state.GetAddressOf());
	m_assert(SUCCEEDED(hr), "failed to create rasterizer state");
}

DXRenderer::~DXRenderer() {
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplSDL2_Shutdown();
}

void DXRenderer::clear(glm::vec4 color) {
	const float clear_color[4] = { color.r, color.g, color.b, color.a };
	D3D.context->ClearRenderTargetView(D3D.back_buffer.Get(), clear_color);
}

void DXRenderer::GUI_new_frame(SDL_Window* window) {
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplSDL2_NewFrame(window);
	ImGui::NewFrame();
}

void DXRenderer::GUI_render() {
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void DXRenderer::render(const Raekor::Mesh& m) {
}

} // namespace Raekor

#pragma once

#include "pch.h"
#include "renderer.h"

namespace Raekor {
// TODO: there's gotta be a better way to store these variables in a way thats
// both platform independent, abstract but still able to get/set by other classes
struct COM_PTRS {
	com_ptr<IDXGISwapChain> swap_chain;
	com_ptr<ID3D11Device> device;
	com_ptr<ID3D11DeviceContext> context;
	com_ptr<ID3D11RenderTargetView> back_buffer;
	com_ptr<ID3D11RasterizerState> rasterize_state;
};

extern COM_PTRS D3D;

class DXRenderer : public Renderer {
public:
	DXRenderer(SDL_Window* window);
	~DXRenderer();
	virtual void clear(glm::vec4 color) override;
	virtual void GUI_new_frame(SDL_Window* window) override;
	virtual void GUI_render() override;
	virtual void render(const Raekor::Mesh& m) override;
};

} // namespace Raekor
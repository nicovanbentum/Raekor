#pragma once

#include "pch.h"
#include "DXDevice.h"
#include "Raekor/scene.h"

namespace Raekor::DX {

class GBufferPass {
	using ShaderLibrary = std::unordered_map<std::string, ComPtr<IDxcBlob>>;

	struct {
		glm::vec4	mAlbedo;
		glm::uvec4	mTextures;
		glm::mat4	mViewProj;
	} mPushConstants;

public:
	void Init(const Viewport& viewport, const ShaderLibrary& shaders, Device& inDevice);
	void Render(const Viewport& inViewport, const Scene& inScene, const Device& inDevice, ID3D12GraphicsCommandList* inCmdList);

	uint32_t m_RenderTarget;
	uint32_t m_RenderTargetView;
	uint32_t m_DepthStencilTarget;

private:
	ComPtr<ID3D12PipelineState> m_Pipeline;
};

} // namespace Raekor::DX

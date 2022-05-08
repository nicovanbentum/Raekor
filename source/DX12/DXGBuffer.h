#pragma once

#include "pch.h"
#include "DXUtil.h"
#include "DXDevice.h"
#include "Raekor/scene.h"

namespace Raekor::DX {

class GBufferPass {
	using ShaderLibrary = std::unordered_map<std::string, ComPtr<IDxcBlob>>;

	struct RootConstants {
		glm::vec4	mAlbedo;
		glm::uvec4	mTextures;
		glm::vec4	mProperties;
		glm::mat4	mViewProj;
	} mRootConstants;

	static_assert(sizeof(mRootConstants) <= sRootSignatureSize);

public:
	void Init(const Viewport& viewport, const ShaderLibrary& shaders, Device& inDevice);
	void Render(const Viewport& inViewport, const Scene& inScene, const Device& inDevice, ID3D12GraphicsCommandList* inCmdList);

	uint32_t m_RenderTarget;
	uint32_t m_RenderTargetSRV;
	uint32_t m_DepthStencil;
	uint32_t m_DepthSRV;

private:
	ComPtr<ID3D12PipelineState> m_Pipeline;
};

} // namespace Raekor::DX

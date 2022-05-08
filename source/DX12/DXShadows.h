#pragma once

#include "pch.h"
#include "DXDevice.h"
#include "Raekor/scene.h"
#include "Raekor/camera.h"

namespace Raekor::DX {

class ShadowPass {
	using ShaderLibrary = std::unordered_map<std::string, ComPtr<IDxcBlob>>;

	struct {
		glm::mat4	invViewProj		= {};
		glm::vec4	mLightDir		= {};
		glm::uvec4	mTextures		= {};
		glm::uvec2  mDispatchSize	= {};
		uint32_t	mFrameCounter	= 0;
	} mRootConstants;

public:
	void Init(const Viewport& inViewport, const ShaderLibrary& inShaders, Device& inDevice);
	void Render(const Viewport& inViewport, const Device& inDevice, const Scene& scene, uint32_t inTLAS, uint32_t inGBuffer, uint32_t inGBufferDepth, ID3D12GraphicsCommandList* inCmdList);

	uint32_t m_ResultTexture;
private:
	ComPtr<ID3D12PipelineState> m_Pipeline;
};

}
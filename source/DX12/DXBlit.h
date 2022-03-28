#pragma once

#include "DXDevice.h"
#include "DXSampler.h"

namespace Raekor::DX {

class BlitPass {
	using ShaderLibrary = std::unordered_map<std::string, ComPtr<IDxcBlob>>;

	struct {
		uint32_t mSourceTextureIndex;
		uint32_t mSampleIndex;
	} mPushConstants;

public:
	void Init(Device& inDevice, const ShaderLibrary& inShaders);
	void Render(Device& inDevice, ID3D12GraphicsCommandList* inCmdList, uint32_t inSrc, uint32_t inDst, ESampler inSampler);

private:
	ComPtr<ID3D12PipelineState> m_Pipeline;
};

}

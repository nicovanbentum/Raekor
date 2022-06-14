#pragma once

#include "DXUtil.h"
#include "DXDevice.h"

namespace Raekor::DX {

class Device;

class DownsamplePass {
	using ShaderLibrary = std::unordered_map<std::string, ComPtr<IDxcBlob>>;

    struct RootConstants {
        uint32_t mips;
        uint32_t numWorkGroups;
        glm::uvec2 workGroupoffset;
        uint32_t globalAtomicIndex;
        uint32_t destImageIndices[13];
    } mRootConstants;

public:
    void Init(Device& inDevice, const ShaderLibrary& inShaders);
    void Render(Device& inDevice, ID3D12GraphicsCommandList* inCmdList, Slice<ResourceID> mipSrvs);

private:
    BufferID m_GlobalAtomicBuffer;
    ComPtr<ID3D12PipelineState> m_Pipeline;
};

}
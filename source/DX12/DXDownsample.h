#pragma once

namespace Raekor::DX {

class Device;

class Downsample {
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
    void Render(Device& inDevice, ID3D12GraphicsCommandList* inCmdList, uint32_t texture, uint32_t mips);

private:
    uint32_t m_GlobalAtomicBuffer;
    ComPtr<ID3D12PipelineState> m_Pipeline;
};

}
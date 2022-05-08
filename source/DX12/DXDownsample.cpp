#include "pch.h"
#include "DXDownsample.h"
#include "DXDevice.h"
#include "DXUtil.h"

namespace Raekor::DX {

void Downsample::Init(Device& inDevice, const ShaderLibrary& inShaders) {
    ComPtr<ID3D12Resource> global_atomic_buffer;
    gThrowIfFailed(inDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t)),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(global_atomic_buffer.GetAddressOf()))
    );

    m_GlobalAtomicBuffer = inDevice.m_CbvSrvUavHeap.AddResource(global_atomic_buffer);
    inDevice.CreateShaderResourceView(m_GlobalAtomicBuffer);

    const auto& compute_shader = inShaders.at("spdCS");

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.CS = CD3DX12_SHADER_BYTECODE(compute_shader->GetBufferPointer(), compute_shader->GetBufferSize());
    desc.pRootSignature = inDevice.GetGlobalRootSignature();

    gThrowIfFailed(inDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(m_Pipeline.GetAddressOf())));
}

void Downsample::Render(Device& inDevice, ID3D12GraphicsCommandList* inCmdList, uint32_t inTextureHandle, uint32_t inMips) {
    const auto& texture = inDevice.m_CbvSrvUavHeap[inTextureHandle];
    const auto rect_info = glm::uvec4(0u, 0u, texture->GetDesc().Width, texture->GetDesc().Height);

    glm::uvec2 work_group_offset, dispatchThreadGroupCountXY, numWorkGroupsAndMips;
    work_group_offset[0] = rect_info[0] / 64; // rectInfo[0] = left
    work_group_offset[1] = rect_info[1] / 64; // rectInfo[1] = top

    auto endIndexX = (rect_info[0] + rect_info[2] - 1) / 64; // rectInfo[0] = left, rectInfo[2] = width
    auto endIndexY = (rect_info[1] + rect_info[3] - 1) / 64; // rectInfo[1] = top, rectInfo[3] = height

    dispatchThreadGroupCountXY[0] = endIndexX + 1 - work_group_offset[0];
    dispatchThreadGroupCountXY[1] = endIndexY + 1 - work_group_offset[1];

    numWorkGroupsAndMips[0] = (dispatchThreadGroupCountXY[0]) * (dispatchThreadGroupCountXY[1]);

    if (inMips >= 0) {
        numWorkGroupsAndMips[1] = inMips;
    }
    else { // calculate based on rect width and height
        auto resolution = glm::max(rect_info[2], rect_info[3]);
        numWorkGroupsAndMips[1] = uint32_t((glm::min(glm::floor(glm::log2(float(resolution))), float(12))));
    }

    mRootConstants.mips = numWorkGroupsAndMips[1];
    mRootConstants.numWorkGroups = numWorkGroupsAndMips[0];
    mRootConstants.workGroupoffset = work_group_offset;
    mRootConstants.destImageIndices[0] = inTextureHandle;
    mRootConstants.globalAtomicIndex = m_GlobalAtomicBuffer;

    inCmdList->SetPipelineState(m_Pipeline.Get());
    inCmdList->SetComputeRootSignature(inDevice.GetGlobalRootSignature());

    const std::array heaps = { inDevice.m_CbvSrvUavHeap.GetHeap() };
    inCmdList->SetDescriptorHeaps(heaps.size(), heaps.data());
    
    inCmdList->SetComputeRoot32BitConstants(0, sizeof(mRootConstants) / sizeof(DWORD), &mRootConstants, 0);
    inCmdList->Dispatch(dispatchThreadGroupCountXY.x, dispatchThreadGroupCountXY.y, 1);
}

} // Raekor
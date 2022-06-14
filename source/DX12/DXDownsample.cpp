#include "pch.h"
#include "DXDownsample.h"
#include "DXDevice.h"
#include "DXUtil.h"

namespace Raekor::DX {

void DownsamplePass::Init(Device& inDevice, const ShaderLibrary& inShaders) {
    m_GlobalAtomicBuffer = inDevice.CreateBuffer(Buffer::Desc{
        .size = sizeof(uint32_t),
        .usage = Buffer::Usage::GENERAL
    });

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srv_desc.Format = DXGI_FORMAT_UNKNOWN;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Buffer.NumElements = 1;
    srv_desc.Buffer.StructureByteStride = sizeof(uint32_t);

    const auto& compute_shader = inShaders.at("spdCS");

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.CS = CD3DX12_SHADER_BYTECODE(compute_shader->GetBufferPointer(), compute_shader->GetBufferSize());
    desc.pRootSignature = inDevice.GetGlobalRootSignature();

    gThrowIfFailed(inDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(m_Pipeline.GetAddressOf())));
}


void DownsamplePass::Render(Device& inDevice, ID3D12GraphicsCommandList* inCmdList, Slice<ResourceID> mipSrvs) {
    const auto mip_count = mipSrvs.Length();
    const auto& resource_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    const auto& texture = resource_heap.Get(mipSrvs[0]);
    const auto rect_info = glm::uvec4(0u, 0u, texture->GetDesc().Width, texture->GetDesc().Height);

    glm::uvec2 work_group_offset, dispatchThreadGroupCountXY, numWorkGroupsAndMips;
    work_group_offset[0] = rect_info[0] / 64; // rectInfo[0] = left
    work_group_offset[1] = rect_info[1] / 64; // rectInfo[1] = top

    auto endIndexX = (rect_info[0] + rect_info[2] - 1) / 64; // rectInfo[0] = left, rectInfo[2] = width
    auto endIndexY = (rect_info[1] + rect_info[3] - 1) / 64; // rectInfo[1] = top, rectInfo[3] = height

    dispatchThreadGroupCountXY[0] = endIndexX + 1 - work_group_offset[0];
    dispatchThreadGroupCountXY[1] = endIndexY + 1 - work_group_offset[1];

    numWorkGroupsAndMips[0] = (dispatchThreadGroupCountXY[0]) * (dispatchThreadGroupCountXY[1]);

    if (mip_count >= 0) {
        numWorkGroupsAndMips[1] = mip_count;
    }
    else { // calculate based on rect width and height
        auto resolution = glm::max(rect_info[2], rect_info[3]);
        numWorkGroupsAndMips[1] = uint32_t((glm::min(glm::floor(glm::log2(float(resolution))), float(12))));
    }

    mRootConstants.mips = numWorkGroupsAndMips[1];
    mRootConstants.numWorkGroups = numWorkGroupsAndMips[0];
    mRootConstants.workGroupoffset = work_group_offset;
    mRootConstants.globalAtomicIndex = inDevice.GetBuffer(m_GlobalAtomicBuffer).GetView().ToIndex();

    for (uint32_t i = 0; i < mip_count; i++)
        mRootConstants.destImageIndices[i] = mipSrvs[i].ToIndex();

    inCmdList->SetPipelineState(m_Pipeline.Get());
    inCmdList->SetComputeRootSignature(inDevice.GetGlobalRootSignature());

    const std::array heaps = { resource_heap.GetHeap()};
    inCmdList->SetDescriptorHeaps(heaps.size(), heaps.data());
    
    inCmdList->SetComputeRoot32BitConstants(0, sizeof(mRootConstants) / sizeof(DWORD), &mRootConstants, 0);
    inCmdList->Dispatch(dispatchThreadGroupCountXY.x, dispatchThreadGroupCountXY.y, 1);
}

} // Raekor
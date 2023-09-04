#include "pch.h"
#include "DXResource.h"

namespace Raekor::DX12 {

D3D12_RESOURCE_STATES gGetResourceStates(Buffer::Usage inUsage)
{
    return D3D12_RESOURCE_STATES();
}


D3D12_RESOURCE_STATES gGetResourceStates(Texture::Usage inUsage)
{
    switch (inUsage)
    {
        case Texture::Usage::RENDER_TARGET:
            return D3D12_RESOURCE_STATE_RENDER_TARGET;
        case Texture::Usage::DEPTH_STENCIL_TARGET:
            return D3D12_RESOURCE_STATE_DEPTH_WRITE;
        case Texture::Usage::GENERAL:
            return D3D12_RESOURCE_STATE_COMMON;
        case Texture::Usage::SHADER_READ_ONLY:
            return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
        case Texture::Usage::SHADER_READ_WRITE:
            return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        default:
            assert(false);
    }

    return D3D12_RESOURCE_STATES();
}


D3D12_DESCRIPTOR_HEAP_TYPE gGetHeapType(Buffer::Usage inUsage)
{
    return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
}


D3D12_DESCRIPTOR_HEAP_TYPE gGetHeapType(Texture::Usage inUsage)
{
    switch (inUsage)
    {
        case Texture::DEPTH_STENCIL_TARGET:
            return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        case Texture::RENDER_TARGET:
            return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        case Texture::SHADER_READ_ONLY:
        case Texture::SHADER_READ_WRITE:
            return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        default:
            assert(false);
    }

    assert(false);
    return D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
}


void DescriptorHeap::Init(ID3D12Device* inDevice, D3D12_DESCRIPTOR_HEAP_TYPE inType, uint32_t inCount, D3D12_DESCRIPTOR_HEAP_FLAGS inFlags)
{
    constexpr auto heap_type_debug_names = std::array
    {
        L"CBV_SRV_UAV_HEAP",
        L"SAMPLER_HEAP",
        L"RTV_HEAP",
        L"DSV_HEAP"
    };

    const auto desc = D3D12_DESCRIPTOR_HEAP_DESC
    {
        .Type = inType,
        .NumDescriptors = inCount,
        .Flags = inFlags
    };

    gThrowIfFailed(inDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_Heap)));
    m_Heap->SetName(heap_type_debug_names[inType]);

    m_HeapPtr = m_Heap->GetCPUDescriptorHandleForHeapStart();
    m_HeapIncrement = inDevice->GetDescriptorHandleIncrementSize(inType);
}


} // namespace Raekor::DX12
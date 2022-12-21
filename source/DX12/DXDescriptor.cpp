#include "pch.h"
#include "DXDescriptor.h"
#include "DXUtil.h"

namespace Raekor::DX {

constexpr auto gHeapTypeDebugNames = std::array {
    L"CBV_SRV_UAV_HEAP",
    L"SAMPLER_HEAP",
    L"RTV_HEAP",
    L"DSV_HEAP"
};

void DescriptorHeap::Init(ID3D12Device* inDevice, D3D12_DESCRIPTOR_HEAP_TYPE inType, uint32_t inCount, D3D12_DESCRIPTOR_HEAP_FLAGS inFlags) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = inType;
    desc.Flags = inFlags;
    desc.NumDescriptors = inCount;

    gThrowIfFailed(inDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_Heap)));
    m_Heap->SetName(gHeapTypeDebugNames[inType]);

    m_HeapPtr = m_Heap->GetCPUDescriptorHandleForHeapStart();
    m_HeapIncrement = inDevice->GetDescriptorHandleIncrementSize(inType);
}

} // Raekor::DX
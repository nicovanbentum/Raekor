#include "pch.h"
#include "DXDescriptor.h"
#include "DXUtil.h"

namespace Raekor::DX12 {

constexpr auto gHeapTypeDebugNames = std::array {
    L"CBV_SRV_UAV_HEAP",
    L"SAMPLER_HEAP",
    L"RTV_HEAP",
    L"DSV_HEAP"
};

void DescriptorHeap::Init(ID3D12Device* inDevice, D3D12_DESCRIPTOR_HEAP_TYPE inType, uint32_t inCount, D3D12_DESCRIPTOR_HEAP_FLAGS inFlags) {
    const auto desc = D3D12_DESCRIPTOR_HEAP_DESC {
        .Type           = inType,
        .NumDescriptors = inCount,
        .Flags          = inFlags
    };

    gThrowIfFailed(inDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_Heap)));
    m_Heap->SetName(gHeapTypeDebugNames[inType]);

    m_HeapPtr = m_Heap->GetCPUDescriptorHandleForHeapStart();
    m_HeapIncrement = inDevice->GetDescriptorHandleIncrementSize(inType);
}

} // Raekor::DX12
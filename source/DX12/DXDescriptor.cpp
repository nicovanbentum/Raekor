#include "pch.h"
#include "DXDescriptor.h"
#include "DXUtil.h"

namespace Raekor::DX {

void DescriptorHeap::Init(ID3D12Device* inDevice, D3D12_DESCRIPTOR_HEAP_TYPE inType, uint32_t inCount, D3D12_DESCRIPTOR_HEAP_FLAGS inFlags) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = inType;
    desc.Flags = inFlags;
    desc.NumDescriptors = inCount;


    gThrowIfFailed(inDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_Heap)));

    m_HeapPtr = m_Heap->GetCPUDescriptorHandleForHeapStart();
    m_HeapIncrement = inDevice->GetDescriptorHandleIncrementSize(inType);
}


size_t DescriptorHeap::AddResource(const ComPtr<ID3D12Resource>& inResource) {
    size_t index;

    if (m_FreeIndices.empty()) {
        emplace_back(inResource);
        index = size() - 1;
    }
    else {
        index = m_FreeIndices.back();
        (*this)[index] = inResource;
        m_FreeIndices.pop_back();
    }

    return index;
}


bool DescriptorHeap::RemoveResource(size_t index) {
    if (index > size() - 1)
        return false;

    m_FreeIndices.push_back(index);
    return true;
}

} // Raekor::DX
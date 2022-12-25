#pragma once

#include "DXResource.h"

namespace Raekor::DX {

class DescriptorHeap : public DescriptorPool {
public:
    void Init(ID3D12Device* inDevice, D3D12_DESCRIPTOR_HEAP_TYPE inType, uint32_t inCount, D3D12_DESCRIPTOR_HEAP_FLAGS inFlags);

    ID3D12DescriptorHeap* GetHeap() const { return m_Heap.Get(); }

    D3D12_CPU_DESCRIPTOR_HANDLE  GetCPUDescriptorHandle(ID inResourceID) const {
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_HeapPtr, inResourceID.ToIndex(), m_HeapIncrement); 
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID inResourceID) const { 
        return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_Heap->GetGPUDescriptorHandleForHeapStart(), inResourceID.ToIndex(), m_HeapIncrement); 
    }

private:
    UINT m_HeapIncrement = 0;
    ComPtr<ID3D12DescriptorHeap> m_Heap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_HeapPtr;
};

} // Raekor::DX

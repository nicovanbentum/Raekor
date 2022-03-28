#pragma once

namespace Raekor::DX {

class DescriptorHeap : protected std::vector<ComPtr<ID3D12Resource>> {
    using Base = std::vector<ComPtr<ID3D12Resource>>;

public:
    void Init(ID3D12Device* inDevice, D3D12_DESCRIPTOR_HEAP_TYPE inType, uint32_t inCount, D3D12_DESCRIPTOR_HEAP_FLAGS inFlags);
    size_t AddResource(const ComPtr<ID3D12Resource>& inResource);
    bool RemoveResource(size_t inIndex);

    ComPtr<ID3D12Resource>& operator[](uint32_t index) { return Base::operator[](index); }
    const ComPtr<ID3D12Resource>& operator[](uint32_t index) const { return Base::operator[](index); }

    ID3D12DescriptorHeap* GetHeap() const { return m_Heap.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE  GetCPUDescriptorHandle(size_t index) const { return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_HeapPtr, index, m_HeapIncrement); }

private:
    UINT m_HeapIncrement = 0;
    std::vector<size_t> m_FreeIndices;
    ComPtr<ID3D12DescriptorHeap> m_Heap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_HeapPtr;
};

} // Raekor::DX

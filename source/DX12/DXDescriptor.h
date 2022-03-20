#pragma once

namespace Raekor::DX {

template<typename T>
class FreeVector : public std::vector<T> {
public:
    virtual size_t Add(const T& t);
    virtual bool Remove(size_t index);

private:
    std::vector<size_t> free;
};


template<D3D12_DESCRIPTOR_HEAP_TYPE Type>
class DescriptorHeap : public FreeVector<ComPtr<ID3D12Resource>> {
public:
    using ResourceType = ComPtr<ID3D12Resource>;

    void Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_FLAGS inFlags);
    virtual size_t Add(const ResourceType& t);

    ComPtr<ID3D12DescriptorHeap> GetHeap()                             { return m_Heap; }
    D3D12_CPU_DESCRIPTOR_HANDLE  GetCPUDescriptorHandle(size_t index)  { return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_HeapPtr, index, m_HeapIncrement); }

private:
    UINT m_HeapIncrement = 0;
    ComPtr<ID3D12DescriptorHeap> m_Heap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_HeapPtr;
};

template<typename T>
inline size_t FreeVector<T>::Add(const T& t) {
    size_t index;

    if (free.empty()) {
        emplace_back(t);
        index = size() - 1;
    }
    else {
        index = free.back();
        (*this)[index] = t;
        free.pop_back();
    }

    return index;
}


template<typename T>
inline bool FreeVector<T>::Remove(size_t index) {
    if (index > size() - 1)
        return false;

    free.push_back(index);
    return true;
}


template<D3D12_DESCRIPTOR_HEAP_TYPE Type>
inline void DescriptorHeap<Type>::Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_FLAGS inFlags) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = Type;
    desc.NumDescriptors = std::numeric_limits<uint16_t>::max();
    desc.Flags = inFlags;

    gThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_Heap)));

    m_HeapPtr = m_Heap->GetCPUDescriptorHandleForHeapStart();
    m_HeapIncrement = device->GetDescriptorHandleIncrementSize(Type);
}


template<D3D12_DESCRIPTOR_HEAP_TYPE Type>
inline size_t DescriptorHeap<Type>::Add(const ResourceType& t) {
    return FreeVector<ResourceType>::Add(t);
}

} // Raekor::DX

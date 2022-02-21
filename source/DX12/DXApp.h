#pragma once

#include "Raekor/application.h"
#include "Raekor/scene.h"

inline void ThrowIfFailed(HRESULT result) {
    if (FAILED(result)) {
        __debugbreak();
    }
}

namespace Raekor::DX {

template<typename T>
class FreeVector : public std::vector<T> {
public:
    virtual size_t push_back_or_insert(const T& t) {
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

    virtual bool remove(size_t index) {
        if (index > size() - 1)
            return false;

        free.push_back(index);
        return true;
    }

private:
    std::vector<size_t> free;
};

class DescriptorHeap : public FreeVector<ComPtr<ID3D12Resource>> {
public:
    using ResourceType = ComPtr<ID3D12Resource>;

    void Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type) {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = type;
        desc.NumDescriptors = std::numeric_limits<uint16_t>::max();

        ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_Heap)));

        m_HeapPtr = m_Heap->GetCPUDescriptorHandleForHeapStart();
        m_HeapIncrement = device->GetDescriptorHandleIncrementSize(type);
    }

    virtual size_t push_back_or_insert(const ResourceType& t) {
        auto index = FreeVector<ResourceType>::push_back_or_insert(t);


    }

private:
    UINT m_HeapIncrement = 0;
    ComPtr<ID3D12DescriptorHeap> m_Heap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_HeapPtr;
};

class DXApp : public Application {
public:
    static constexpr uint32_t FrameCount = 2;

    DXApp();
    ~DXApp();

    virtual void OnUpdate(float dt) override;
    virtual void OnEvent(const SDL_Event& event) override;

private:
    Scene m_Scene;
    Assets m_Assets;

    ComPtr<IDXGISwapChain3> m_Swapchain;
    ComPtr<ID3D12Device> m_Device;
    std::vector<ComPtr<ID3D12Resource>> m_RenderTargets;
    std::vector<ComPtr<ID3D12Resource>> m_RenderTargetDepths;
    std::vector<ComPtr<ID3D12GraphicsCommandList>> m_CommandLists;
    std::vector<ComPtr<ID3D12CommandAllocator>> m_CommnadAllocators;
    ComPtr<ID3D12CommandQueue> m_Queue;
    ComPtr<ID3D12DescriptorHeap> m_RtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_DsvHeap;
    ComPtr<ID3D12DescriptorHeap> m_SrvCbvUavHeap;
    ComPtr<ID3D12PipelineState> m_Pipeline;

    ComPtr<ID3D12RootSignature> m_RootSignature;

    FreeVector<ComPtr<ID3D12Resource>> m_Buffers;
    std::unordered_map<std::string, ComPtr<IDxcBlob>> m_Shaders;
    
    uint32_t m_FrameIndex = 0;
    HANDLE m_FenceEvent;
    ComPtr<ID3D12Fence> m_Fence;
    std::vector<UINT64> m_FenceValues;
};


}
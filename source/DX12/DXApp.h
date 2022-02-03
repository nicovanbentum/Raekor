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
    size_t push_back_or_insert(const T& t) {
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

    bool remove(size_t index) {
        free.push_back(index);
    }

private:
    std::vector<size_t> free;
};


class DXApp : public Application {
public:
    static constexpr uint32_t FrameCount = 2;

    DXApp();
    ~DXApp();

    virtual void onUpdate(float dt) override;
    virtual void onEvent(const SDL_Event& event) override;

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
    ComPtr<ID3D12PipelineState> m_Pipeline;

    ComPtr<ID3DBlob> m_PixelShader;
    ComPtr<ID3DBlob> m_VertexShader;
    ComPtr<ID3D12RootSignature> m_RootSignature;

    FreeVector<ComPtr<ID3D12Resource>> m_Buffers;
    
    uint32_t m_FrameIndex = 0;
    HANDLE m_FenceEvent;
    ComPtr<ID3D12Fence> m_Fence;
    std::vector<UINT64> m_FenceValues;
};


}
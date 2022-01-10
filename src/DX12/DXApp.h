#pragma once

#include "application.h"

inline void ThrowIfFailed(HRESULT result) {
    if (FAILED(result)) {
        __debugbreak();
    }
}

namespace Raekor::DX {

class DXApp : public Application {
public:
    static constexpr uint32_t FrameCount = 2;

    DXApp();
    ~DXApp();

    virtual void onUpdate(float dt) override;
    virtual void onEvent(const SDL_Event& event) override;

private:
    ComPtr<IDXGISwapChain3> mSwapchain;
    ComPtr<ID3D12Device> mDevice;
    std::vector<ComPtr<ID3D12Resource>> mRenderTargets;
    std::vector<ComPtr<ID3D12GraphicsCommandList>> mCommandLists;
    std::vector<ComPtr<ID3D12CommandAllocator>> mCommandAllocators;
    ComPtr<ID3D12CommandQueue> mQueue;
    ComPtr<ID3D12DescriptorHeap> mRtvHeap;
    
    uint32_t mFrameIndex = 0;
    HANDLE mFenceEvent;
    ComPtr<ID3D12Fence> mFence;
    std::vector<UINT64> mFenceValues;
};


}
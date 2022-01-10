#include "pch.h"
#include "DXApp.h"

namespace Raekor::DX {

DXApp::DXApp() : Application(RendererFlags::NONE) {
    
    ComPtr<ID3D12Debug> mDebug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&mDebug)))) {
        mDebug->EnableDebugLayer();
    }

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory)));

    ComPtr<IDXGIAdapter1> mAdapter;
    ThrowIfFailed(factory->EnumAdapters1(0, &mAdapter));

    ThrowIfFailed(D3D12CreateDevice(mAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&mDevice)));

    const D3D12_COMMAND_QUEUE_DESC qd = {};
    ThrowIfFailed(mDevice->CreateCommandQueue(&qd, IID_PPV_ARGS(&mQueue)));

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.BufferCount = FrameCount;
    desc.Width = viewport.size.x;
    desc.Height = viewport.size.y;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapchain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(mQueue.Get(), wmInfo.info.win.window, &desc, nullptr, nullptr, &swapchain));

    ThrowIfFailed(swapchain.As(&mSwapchain));

    mFrameIndex = mSwapchain->GetCurrentBackBufferIndex();

    D3D12_DESCRIPTOR_HEAP_DESC heap = {};
    heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap.NumDescriptors = FrameCount;

    ThrowIfFailed(mDevice->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&mRtvHeap)));
    auto heapPtr = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart());

    mFenceValues.resize(FrameCount);
    mCommandLists.resize(FrameCount);
    mRenderTargets.resize(FrameCount);
    mCommandAllocators.resize(FrameCount);

    for (uint32_t i = 0; i < FrameCount; i++) {
        ThrowIfFailed(mSwapchain->GetBuffer(i, IID_PPV_ARGS(&mRenderTargets[i])));
        
        // initialize the render target at the heap pointer location
        mDevice->CreateRenderTargetView(mRenderTargets[i].Get(), nullptr, heapPtr);
        
        // increment the heap pointer
        heapPtr.Offset(mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
        
        ThrowIfFailed(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocators[i])));
        ThrowIfFailed(mDevice->CreateCommandList(0x00, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocators[i].Get(), nullptr, IID_PPV_ARGS(&mCommandLists[i])));
        mCommandLists[i]->Close();
    }

    mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence));

    mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (!mFenceEvent) {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
}


DXApp::~DXApp() {
    for (const auto& fenceValue : mFenceValues) {
        ThrowIfFailed(mQueue->Signal(mFence.Get(), fenceValue));

        if (mFence->GetCompletedValue() < fenceValue) {
            ThrowIfFailed(mFence->SetEventOnCompletion(fenceValue, mFenceEvent));
            WaitForSingleObjectEx(mFenceEvent, INFINITE, FALSE);
        }
    }
}


void DXApp::onUpdate(float dt) {
    ThrowIfFailed(mSwapchain->Present(0, 0));

    const UINT64 currentFenceValue = mFenceValues[mFrameIndex];

    ThrowIfFailed(mQueue->Signal(mFence.Get(), currentFenceValue));

    mFrameIndex = mSwapchain->GetCurrentBackBufferIndex();

    if (mFence->GetCompletedValue() < mFenceValues[mFrameIndex]) {
        ThrowIfFailed(mFence->SetEventOnCompletion(mFenceValues[mFrameIndex], mFenceEvent));
        WaitForSingleObjectEx(mFenceEvent, INFINITE, FALSE);
    }

    const auto& cmdList = mCommandLists[mFrameIndex];

    cmdList->Reset(mCommandAllocators[mFrameIndex].Get(), nullptr);
    
    const auto clearColour = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);

    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRenderTargets[mFrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));
    
    const auto rtvPtr = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart()).Offset(mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * mFrameIndex);
    
    cmdList->ClearRenderTargetView(rtvPtr, glm::value_ptr(clearColour), 1, &CD3DX12_RECT(0, 0, viewport.size.x, viewport.size.y));

    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRenderTargets[mFrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON));

    cmdList->Close();

    const std::array commandLists = { static_cast<ID3D12CommandList*>(cmdList.Get()) };
    mQueue->ExecuteCommandLists(commandLists.size(), commandLists.data());

    mFenceValues[mFrameIndex] = currentFenceValue + 1;
}


void DXApp::onEvent(const SDL_Event& event) {
    if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
            while (1) {
                SDL_Event ev;
                SDL_PollEvent(&ev);

                if (ev.window.event == SDL_WINDOWEVENT_RESTORED) {
                    break;
                }
            }
        }
        if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
            if (SDL_GetWindowID(window) == event.window.windowID) {
                running = false;
            }
        }
        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
            const UINT64 currentFenceValue = mFenceValues[mFrameIndex];

            ThrowIfFailed(mQueue->Signal(mFence.Get(), currentFenceValue));

            if (mFence->GetCompletedValue() < mFenceValues[mFrameIndex]) {
                ThrowIfFailed(mFence->SetEventOnCompletion(mFenceValues[mFrameIndex], mFenceEvent));
                WaitForSingleObjectEx(mFenceEvent, INFINITE, FALSE);
            }

            for (auto& renderTarget : mRenderTargets) {
                renderTarget.Reset();
            }

            mRenderTargets.resize(FrameCount);

            ThrowIfFailed(mSwapchain->ResizeBuffers(
                FrameCount, viewport.size.x, viewport.size.y, 
                DXGI_FORMAT_B8G8R8A8_UNORM, 0
            ));

            auto heapPtr = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart());


            for (uint32_t i = 0; i < FrameCount; i++) {
                ThrowIfFailed(mSwapchain->GetBuffer(i, IID_PPV_ARGS(&mRenderTargets[i])));
                mDevice->CreateRenderTargetView(mRenderTargets[i].Get(), nullptr, heapPtr);
                heapPtr.Offset(mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
            }
        }
    }
}

}
#include "pch.h"
#include "DXDevice.h"
#include "DXUtil.h"

namespace Raekor::DX {

Device::Device(SDL_Window* window) {
    UINT device_creation_flags = 0;

#ifndef NDEBUG
    ComPtr<ID3D12Debug1> mDebug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&mDebug)))) {
        mDebug->EnableDebugLayer();
    }

    device_creation_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    ComPtr<IDXGIFactory4> factory;
    gThrowIfFailed(CreateDXGIFactory2(device_creation_flags, IID_PPV_ARGS(&factory)));

    gThrowIfFailed(factory->EnumAdapters1(0, &m_Adapter));

    gThrowIfFailed(D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&m_Device)));

    //D3D12MA::ALLOCATOR_DESC allocator_desc = {};
    //allocator_desc.pAdapter = m_Adapter.Get();
    //allocator_desc.pDevice = m_Device.Get();
    //gThrowIfFailed(D3D12MA::CreateAllocator(&allocator_desc, &m_Allocator));

    const D3D12_COMMAND_QUEUE_DESC qd = {};
    gThrowIfFailed(m_Device->CreateCommandQueue(&qd, IID_PPV_ARGS(&m_Queue)));

    m_RtvHeap.Init(m_Device.Get(), D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    m_DsvHeap.Init(m_Device.Get(), D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    m_CbvSrvUavHeap.Init(m_Device.Get(), D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
}

}
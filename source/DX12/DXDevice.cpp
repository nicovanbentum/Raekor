#include "pch.h"
#include "DXDevice.h"
#include "DXSampler.h"
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

    D3D12MA::ALLOCATOR_DESC allocator_desc = {};
    allocator_desc.pAdapter = m_Adapter.Get();
    allocator_desc.pDevice = m_Device.Get();
    gThrowIfFailed(D3D12MA::CreateAllocator(&allocator_desc, &m_Allocator));

    const D3D12_COMMAND_QUEUE_DESC qd = {};
    gThrowIfFailed(m_Device->CreateCommandQueue(&qd, IID_PPV_ARGS(&m_Queue)));

    m_RtvHeap.Init(       m_Device.Get(),  D3D12_DESCRIPTOR_HEAP_TYPE_RTV,          std::numeric_limits<uint16_t>::max(),   D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    m_DsvHeap.Init(       m_Device.Get(),  D3D12_DESCRIPTOR_HEAP_TYPE_DSV,          std::numeric_limits<uint16_t>::max(),   D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    m_SamplerHeap.Init(   m_Device.Get(),  D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,      ESampler::Limit,                        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    m_CbvSrvUavHeap.Init( m_Device.Get(),  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,  std::numeric_limits<uint16_t>::max(),   D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

    for (size_t sampler_index = 0; sampler_index < ESampler::Count; sampler_index++)
        m_Device->CreateSampler(&SAMPLER_DESC[sampler_index], m_SamplerHeap.GetCPUDescriptorHandle(sampler_index));

    D3D12_ROOT_PARAMETER1 constants_param = {};
    constants_param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    constants_param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    constants_param.Constants.ShaderRegister = 0;
    constants_param.Constants.RegisterSpace = 0;
    constants_param.Constants.Num32BitValues = sRootSignatureSize / sizeof(DWORD);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC vrsd;
    vrsd.Init_1_1(1, &constants_param, ESampler::Count, STATIC_SAMPLER_DESC.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | 
                                                                                    D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED  | 
                                                                                    D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED);
    ComPtr<ID3DBlob> signature, error;
    auto serialize_vrs_hr = D3DX12SerializeVersionedRootSignature(&vrsd, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);

    if (error)
        OutputDebugStringA((char*)error->GetBufferPointer());

    gThrowIfFailed(serialize_vrs_hr);
    gThrowIfFailed(m_Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_GlobalRootSignature)));
}

void Device::CreateDepthStencilView(uint32_t inIndex, D3D12_DEPTH_STENCIL_VIEW_DESC* inDesc) {
    m_Device->CreateDepthStencilView(m_DsvHeap[inIndex].Get(), inDesc, m_DsvHeap.GetCPUDescriptorHandle(inIndex));
}

void Device::CreateRenderTargetView(uint32_t inIndex, D3D12_RENDER_TARGET_VIEW_DESC* inDesc) {
    m_Device->CreateRenderTargetView(m_RtvHeap[inIndex].Get(), inDesc, m_RtvHeap.GetCPUDescriptorHandle(inIndex));
}

void Device::CreateShaderResourceView(uint32_t inIndex, D3D12_SHADER_RESOURCE_VIEW_DESC* inDesc) {
    m_Device->CreateShaderResourceView(m_CbvSrvUavHeap[inIndex].Get(), inDesc, m_CbvSrvUavHeap.GetCPUDescriptorHandle(inIndex));
}

void Device::CreateUnorderedAccessView(uint32_t inIndex, D3D12_UNORDERED_ACCESS_VIEW_DESC* inDesc) {
    m_Device->CreateUnorderedAccessView(m_CbvSrvUavHeap[inIndex].Get(), nullptr, inDesc, m_CbvSrvUavHeap.GetCPUDescriptorHandle(inIndex));
}

}
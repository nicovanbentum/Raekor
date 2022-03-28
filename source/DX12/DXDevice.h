#pragma once

#include "DXDescriptor.h"

namespace Raekor::DX {

class Device {
public:
	Device(SDL_Window* window);

	ID3D12Device* GetRaw() { return m_Device.Get(); }
	ID3D12Device* operator-> () { return m_Device.Get(); }
	ID3D12CommandQueue* GetQueue() { return m_Queue.Get(); }
	ID3D12RootSignature* GetGlobalRootSignature() const { return m_GlobalRootSignature.Get(); }

public:
	ComPtr<ID3D12Device> m_Device;
	ComPtr<IDXGIAdapter1> m_Adapter;
	ComPtr<ID3D12CommandQueue> m_Queue;
	ComPtr<D3D12MA::Allocator> m_Allocator;

	ComPtr<ID3D12RootSignature> m_GlobalRootSignature;
	DescriptorHeap m_RtvHeap;
	DescriptorHeap m_DsvHeap;
	DescriptorHeap m_SamplerHeap;
	DescriptorHeap m_CbvSrvUavHeap;
};

}

#pragma once

#include "DXDescriptor.h"

namespace Raekor::DX {

class Device {
public:
	Device(SDL_Window* window);

	ID3D12Device* GetRaw() { return m_Device.Get(); }
	ID3D12Device* operator-> () { return m_Device.Get(); }
	ID3D12CommandQueue* GetQueue() { return m_Queue.Get(); }

public:
	ComPtr<ID3D12Device> m_Device;
	ComPtr<IDXGIAdapter1> m_Adapter;
	ComPtr<ID3D12CommandQueue> m_Queue;
	ComPtr<D3D12MA::Allocator> m_Allocator;


	DescriptorHeap<D3D12_DESCRIPTOR_HEAP_TYPE_RTV> m_RtvHeap;
	DescriptorHeap<D3D12_DESCRIPTOR_HEAP_TYPE_DSV> m_DsvHeap;
	DescriptorHeap<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV> m_CbvSrvUavHeap;
};

}

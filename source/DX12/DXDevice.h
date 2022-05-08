#pragma once

#include "DXDescriptor.h"

namespace Raekor::DX {

class Device {
public:
	Device(SDL_Window* window);

	ID3D12Device5* operator-> () { return m_Device.Get(); }
	operator ID3D12Device5*() const { return m_Device.Get(); }

	ID3D12CommandQueue* GetQueue() { return m_Queue.Get(); }
	ID3D12RootSignature* GetGlobalRootSignature() const { return m_GlobalRootSignature.Get(); }

	void CreateDepthStencilView(uint32_t inIndex, D3D12_DEPTH_STENCIL_VIEW_DESC* inDesc = nullptr);
	void CreateRenderTargetView(uint32_t inIndex, D3D12_RENDER_TARGET_VIEW_DESC* inDesc = nullptr);
	void CreateShaderResourceView(uint32_t inIndex, D3D12_SHADER_RESOURCE_VIEW_DESC* inDesc = nullptr);
	void CreateUnorderedAccessView(uint32_t inIndex, D3D12_UNORDERED_ACCESS_VIEW_DESC* inDesc = nullptr);

public:
	ComPtr<ID3D12Device5> m_Device;
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

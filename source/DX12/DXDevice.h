#pragma once

#include "DXResource.h"
#include "DXDescriptor.h"

namespace Raekor::DX {

class Device {
public:
	Device(SDL_Window* window);

	ID3D12Device5* operator-> () { return m_Device.Get(); }
	operator ID3D12Device5*() const { return m_Device.Get(); }

	ID3D12CommandQueue*  GetQueue() { return m_Queue.Get(); }
	ID3D12RootSignature* GetGlobalRootSignature() const { return m_GlobalRootSignature.Get(); }
	const DescriptorHeap& GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE inType) const { return m_Heaps[inType]; }

	[[nodiscard]] BufferID  CreateBuffer(const Buffer::Desc& desc, const std::wstring& name = std::wstring());
	[[nodiscard]] TextureID CreateTexture(const Texture::Desc& desc, const std::wstring& name = std::wstring());

	[[nodiscard]] BufferID CreateBufferView(BufferID inTextureID, const Buffer::Desc& desc);
	[[nodiscard]] TextureID CreateTextureView(TextureID inTextureID, const Texture::Desc& desc);

	void ReleaseBuffer(BufferID inID)   { m_Buffers.Remove(inID);  }
	void ReleaseTexture(TextureID inID) { m_Textures.Remove(inID); }
	
	Buffer& GetBuffer(BufferID inID) { return m_Buffers.Get(inID); }
	const Buffer& GetBuffer(BufferID inID) const { return m_Buffers.Get(inID); }

	Texture& GetTexture(TextureID inID) { return m_Textures.Get(inID); }
	const Texture& GetTexture(TextureID inID) const { return m_Textures.Get(inID); }

	ResourceID CreateDepthStencilView(ResourceRef inResourceID, D3D12_DEPTH_STENCIL_VIEW_DESC* inDesc = nullptr);
	ResourceID CreateRenderTargetView(ResourceRef inResourceID, D3D12_RENDER_TARGET_VIEW_DESC* inDesc = nullptr);
	ResourceID CreateShaderResourceView(ResourceRef inResourceID, D3D12_SHADER_RESOURCE_VIEW_DESC* inDesc = nullptr);
	ResourceID CreateUnorderedAccessView(ResourceRef inResourceID, D3D12_UNORDERED_ACCESS_VIEW_DESC* inDesc = nullptr);

	void ReleaseDepthStencilView(ResourceID inResourceID) { m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Remove(inResourceID); }
	void ReleaseRenderTargetView(ResourceID inResourceID) { m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Remove(inResourceID); }
	void ReleaseShaderResourceView(ResourceID inResourceID) { m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Remove(inResourceID); }
	void ReleaseUnorderedAccessView(ResourceID inResourceID) { m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Remove(inResourceID); }

public:
	ComPtr<ID3D12Device5> m_Device;
	ComPtr<IDXGIAdapter1> m_Adapter;
	ComPtr<ID3D12CommandQueue> m_Queue;
	ComPtr<ID3D12CommandQueue> m_CopyQueue;
	ComPtr<D3D12MA::Allocator> m_Allocator;
	ComPtr<ID3D12RootSignature> m_GlobalRootSignature;
	std::array<DescriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> m_Heaps;

private:
	Buffer::Pool	m_Buffers;
	Texture::Pool	m_Textures;
};


class StagingHeap {
private:
	struct StagingBuffer {
		size_t mSize		= 0;
		size_t mCapacity	= 0;
		bool mRetired		= true;
		uint8_t* mPtr		= nullptr;
		BufferID mBufferID	= BufferID();
	};

public:
	StagingHeap(Device& inDevice) : m_Device(inDevice) {}
	~StagingHeap();

	void StageBuffer(ID3D12GraphicsCommandList* inCmdList, ResourceRef inResource, size_t inOffset, const void* inData, size_t inSize);
	void StageTexture(ID3D12GraphicsCommandList* inCmdList, ResourceRef inResource, size_t inSubResource, const void* inData);

private:
	Device& m_Device;
	std::vector<StagingBuffer> m_Buffers;
};

}

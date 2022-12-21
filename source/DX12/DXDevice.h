#pragma once

#include "DXResource.h"
#include "DXDescriptor.h"

namespace Raekor::DX {

class CommandList;
class IRenderPass;
struct TextureResource;

class Device : public INoCopyNoMove {
public:
	Device(SDL_Window* window, uint32_t inFrameCount);

	operator ID3D12Device5*()					{ return m_Device.Get(); }
	operator const ID3D12Device5* () const		{ return m_Device.Get(); }
	ID3D12Device5* operator-> ()				{ return m_Device.Get(); }
	const ID3D12Device5* operator-> () const	{ return m_Device.Get(); }

	[[nodiscard]] ID3D12CommandQueue*  GetQueue() const												{ return m_Queue.Get(); }
	[[nodiscard]] ID3D12RootSignature* GetGlobalRootSignature() const								{ return m_GlobalRootSignature.Get(); }
	[[nodiscard]] const DescriptorHeap& GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE inType) const	{ return m_Heaps[inType]; }

	void BindDrawDefaults(CommandList& inCmdList);
	void Submit(const Slice<CommandList>& inCmdLists);

	[[nodiscard]] BufferID  CreateBuffer(const Buffer::Desc& inDesc, const std::wstring& inName = std::wstring());
	[[nodiscard]] TextureID CreateTexture(const Texture::Desc& inDesc, const std::wstring& inName = std::wstring());

	[[nodiscard]] BufferID  CreateBufferView(BufferID inTextureID, const Buffer::Desc& inDesc);
	[[nodiscard]] TextureID CreateTextureView(TextureID inTextureID, const Texture::Desc& inDesc);
	[[nodiscard]] TextureID CreateTextureView(ResourceRef inResource, const Texture::Desc& inDesc);

	void ReleaseBuffer(BufferID inID)   { assert(inID.Isvalid()); m_Buffers.Remove(inID);  }
	void ReleaseTexture(TextureID inID) { assert(inID.Isvalid()); m_Textures.Remove(inID); }
	
	[[nodiscard]] Buffer& GetBuffer(BufferID inID) { assert(inID.Isvalid()); return m_Buffers.Get(inID); }
	[[nodiscard]] const Buffer& GetBuffer(BufferID inID) const { assert(inID.Isvalid()); return m_Buffers.Get(inID); }

	[[nodiscard]] Texture& GetTexture(TextureID inID) { assert(inID.Isvalid()); return m_Textures.Get(inID); }
	[[nodiscard]] const Texture& GetTexture(TextureID inID) const { assert(inID.Isvalid()); return m_Textures.Get(inID); }

	[[nodiscard]] ID3D12Resource* GetResourcePtr(BufferID inID)  { return GetBuffer(inID).GetResource().Get(); }
	[[nodiscard]] ID3D12Resource* GetResourcePtr(TextureID inID) { return GetTexture(inID).GetResource().Get(); }

	[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(BufferID inID);
	[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(TextureID inID);
	[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetHeapPtr(TextureResource inResource);


	[[nodiscard]] DescriptorID CreateDepthStencilView(ResourceRef inResourceID, D3D12_DEPTH_STENCIL_VIEW_DESC* inDesc = nullptr);
	[[nodiscard]] DescriptorID CreateRenderTargetView(ResourceRef inResourceID, D3D12_RENDER_TARGET_VIEW_DESC* inDesc = nullptr);
	[[nodiscard]] DescriptorID CreateShaderResourceView(ResourceRef inResourceID, D3D12_SHADER_RESOURCE_VIEW_DESC* inDesc = nullptr);
	[[nodiscard]] DescriptorID CreateUnorderedAccessView(ResourceRef inResourceID, D3D12_UNORDERED_ACCESS_VIEW_DESC* inDesc = nullptr);

	void ReleaseDepthStencilView(DescriptorID inResourceID)	 { m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Remove(inResourceID); }
	void ReleaseRenderTargetView(DescriptorID inResourceID)    { m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Remove(inResourceID); }
	void ReleaseShaderResourceView(DescriptorID inResourceID)  { m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Remove(inResourceID); }
	void ReleaseUnorderedAccessView(DescriptorID inResourceID) { m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Remove(inResourceID); }

	[[nodiscard]] D3D12_COMPUTE_PIPELINE_STATE_DESC  CreatePipelineStateDesc(IRenderPass* inRenderPass, const std::string& inComputeShader);
	[[nodiscard]] D3D12_GRAPHICS_PIPELINE_STATE_DESC CreatePipelineStateDesc(IRenderPass* inRenderPass, const std::string& inVertexShader, const std::string& inPixelShader);

	[[nodiscard]] uint32_t GetBindlessHeapIndex(DescriptorID inResource)	{ return inResource.ToIndex(); }

private:
	void CreateDescriptor(TextureID inID, const Texture::Desc& inDesc);

public:
	ComPtr<ID3D12Device5> m_Device;
	ComPtr<IDXGIAdapter1> m_Adapter;
	ComPtr<ID3D12CommandQueue> m_Queue;
	ComPtr<ID3D12CommandQueue> m_CopyQueue;
	ComPtr<D3D12MA::Allocator> m_Allocator;
	ComPtr<ID3D12RootSignature> m_GlobalRootSignature;
	std::array<DescriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> m_Heaps;
	std::unordered_map<std::string, ComPtr<IDxcBlob>> m_Shaders;

private:
	uint32_t m_NumFrames;
	Buffer::Pool m_Buffers;
	Texture::Pool m_Textures;
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

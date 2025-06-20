#pragma once

#include "Shared.h"
#include "Resource.h"
#include "Application.h"

namespace RK::DX12 {

class CommandList;
class IRenderPass;
class ComputeProgram;
class GraphicsProgram;

class Device
{
public:
    explicit Device(Application* inApp);
    ~Device();

    ID3D12Device5* operator* ()              { return m_Device.Get(); }
    const ID3D12Device5* operator* () const  { return m_Device.Get(); }
    ID3D12Device5* operator-> ()             { return m_Device.Get(); }
    const ID3D12Device5* operator-> () const { return m_Device.Get(); }

    void OnUpdate();

    uint32_t GetFrameIndex() const { return m_FrameIndex; }
    uint32_t GetFrameCounter() const { return m_FrameCounter; }

    [[nodiscard]] bool IsDLSSSupported() const { return mIsDLSSSupported; }
    [[nodiscard]] bool IsDLSSInitialized() const { return mIsDLSSInitialized; }
    [[nodiscard]] bool IsTearingSupported() const { return mIsTearingSupported; }
    [[nodiscard]] bool IsRayTracingSupported() const { return mIsRayTracingSupported; }

    [[nodiscard]] IDXGIAdapter1* GetAdapter() { return m_Adapter.Get(); }
    [[nodiscard]] D3D12MA::Allocator* GetAllocator() { return m_Allocator.Get(); }
    
    [[nodiscard]] ID3D12CommandQueue* GetCopyQueue() { return m_CopyQueue.Get(); }
    [[nodiscard]] ID3D12CommandQueue* GetComputeQueue() { return m_ComputeQueue.Get(); }
    [[nodiscard]] ID3D12CommandQueue* GetGraphicsQueue() { return m_GraphicsQueue.Get(); }

    [[nodiscard]] DescriptorHeap& GetClearHeap() { return m_ClearHeap; }
    [[nodiscard]] DescriptorHeap& GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE inType) { return m_Heaps[inType]; }

    [[nodiscard]] ID3D12RootSignature* GetGlobalRootSignature() { return m_GlobalRootSignature.Get(); }
    [[nodiscard]] ID3D12CommandSignature* GetCommandSignature(ECommandSignature inCmdSig) { return m_CommandSignatures[inCmdSig].Get(); }

    [[nodiscard]] BufferID  CreateBuffer(const Buffer::Desc& inDesc);
    [[nodiscard]] TextureID CreateTexture(const Texture::Desc& inDesc);

    [[nodiscard]] BufferID  CreateBuffer(const Buffer::Desc& inDesc, const Buffer& inBuffer);
    [[nodiscard]] TextureID CreateTexture(const Texture::Desc& inDesc, const Texture& inTexture);

    [[nodiscard]] BufferID  CreateBufferView(BufferID inTextureID, const Buffer::Desc& inDesc);
    [[nodiscard]] TextureID CreateTextureView(TextureID inTextureID, const Texture::Desc& inDesc);
    [[nodiscard]] TextureID CreateTextureView(ID3D12Resource* inResource, const Texture::Desc& inDesc);

    void UploadBufferData(CommandList& inCmdList, Buffer& inBuffer, uint32_t inOffset, const void* inData, uint32_t inSize);
    void UploadTextureData(Texture& inTexture, uint32_t inMip, uint32_t inLayer, uint32_t inROwPitch, const void* inData);

    void FlushUploads(CommandList& inCmdList);
    void RetireUploadBuffers(CommandList& inCmdList);

    /* Safe to call anywhere, will defer the actual release.  */
    void ReleaseBuffer(BufferID inID);
    /* Safe to call anywhere, will defer the actual release.  */
    void ReleaseTexture(TextureID inID);

    /* USE WITH CAUTION. WILL IMMEDIATELY DELETE THE RESOURCE. ONLY USE WHEN YOU KNOW THE GPU IS NO LONGER USING THE RESOURCE!! */
    void ReleaseBufferImmediate(BufferID inID);
    /* USE WITH CAUTION. WILL IMMEDIATELY DELETE THE RESOURCE. ONLY USE WHEN YOU KNOW THE GPU IS NO LONGER USING THE RESOURCE!! */
    void ReleaseTextureImmediate(TextureID inID);

    [[nodiscard]] Buffer& GetBuffer(BufferID inID) { assert(inID.IsValid()); return m_Buffers.Get(inID); }
    [[nodiscard]] const Buffer& GetBuffer(BufferID inID) const { assert(inID.IsValid()); return m_Buffers.Get(inID); }

    [[nodiscard]] Texture& GetTexture(TextureID inID) { assert(inID.IsValid()); return m_Textures.Get(inID); }
    [[nodiscard]] const Texture& GetTexture(TextureID inID) const { assert(inID.IsValid()); return m_Textures.Get(inID); }

    [[nodiscard]] ID3D12Resource* GetD3D12Resource(BufferID inID) { return GetBuffer(inID).GetD3D12Resource(); }
    [[nodiscard]] ID3D12Resource* GetD3D12Resource(TextureID inID) { return GetTexture(inID).GetD3D12Resource(); }

    [[nodiscard]] const ID3D12Resource* GetD3D12Resource(BufferID inID) const { return GetBuffer(inID).GetD3D12Resource(); }
    [[nodiscard]] const ID3D12Resource* GetD3D12Resource(TextureID inID) const { return GetTexture(inID).GetD3D12Resource(); }

    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(BufferID inID);
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(TextureID inID);

    [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(BufferID inID);
    [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(TextureID inID);

    [[nodiscard]] DescriptorID CreateDepthStencilView(ID3D12Resource* inD3D12Resource, const D3D12_DEPTH_STENCIL_VIEW_DESC* inDesc = nullptr);
    [[nodiscard]] DescriptorID CreateRenderTargetView(ID3D12Resource* inD3D12Resource, const D3D12_RENDER_TARGET_VIEW_DESC* inDesc = nullptr);
    [[nodiscard]] DescriptorID CreateShaderResourceView(ID3D12Resource* inD3D12Resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* inDesc = nullptr);
    [[nodiscard]] DescriptorID CreateUnorderedAccessView(ID3D12Resource* inD3D12Resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* inDesc = nullptr);

    void ReleaseDepthStencilView(DescriptorID inDescriptorID) { m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Remove(inDescriptorID); }
    void ReleaseRenderTargetView(DescriptorID inDescriptorID) { m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Remove(inDescriptorID); }
    void ReleaseShaderResourceView(DescriptorID inDescriptorID) { m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Remove(inDescriptorID); }
    void ReleaseUnorderedAccessView(DescriptorID inDescriptorID) { m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Remove(inDescriptorID); }

    [[nodiscard]] uint32_t GetBindlessHeapIndex(DescriptorID inID) const { return inID.GetIndex(); }
    [[nodiscard]] uint32_t GetBindlessHeapIndex(BufferID inID) const { return GetBindlessHeapIndex(GetBuffer(inID).GetDescriptor()); }
    [[nodiscard]] uint32_t GetBindlessHeapIndex(TextureID inID) const { return GetBindlessHeapIndex(GetTexture(inID).GetView()); }

    [[nodiscard]] const Buffer::Pool& GetBufferPool() const { return m_Buffers; }
    [[nodiscard]] const Texture::Pool& GetTexturePool() const { return m_Textures; }

    [[nodiscard]] uint64_t GetUploadBufferSize() const { return m_UploadBuffersSize; }
    [[nodiscard]] const Array<UploadBuffer> GetUploadBuffers() const { return m_UploadBuffers; }

private:
    void CreateDescriptor(BufferID inBufferID, const Buffer::Desc& inDesc);
    void CreateDescriptor(TextureID inTextureID, const Texture::Desc& inDesc);
    void ReleaseDescriptor(Buffer::Usage inUsage, DescriptorID inDescriptorID);
    void ReleaseDescriptor(Texture::Usage inUsage, DescriptorID inDescriptorID);

    /* USE WITH CAUTION. ONLY USE WHEN YOU KNOW THE GPU IS NO LONGER USING THE RESOURCE!! */
    void ReleaseDescriptorImmediate(Buffer::Usage inUsage, DescriptorID inDescriptorID);
    /* USE WITH CAUTION. ONLY USE WHEN YOU KNOW THE GPU IS NO LONGER USING THE RESOURCE!! */
    void ReleaseDescriptorImmediate(Texture::Usage inUsage, DescriptorID inDescriptorID);

private:
    uint32_t m_FrameIndex = 0;
    uint64_t m_FrameCounter = 0;

    BOOL mIsDLSSSupported = false;
    BOOL mIsDLSSInitialized = false;
    BOOL mIsTearingSupported = false;
    BOOL mIsRayTracingSupported = false;

    ComPtr<ID3D12Device5> m_Device;
    ComPtr<IDXGIAdapter1> m_Adapter;
    ComPtr<D3D12MA::Allocator> m_Allocator;
    ComPtr<ID3D12CommandQueue> m_CopyQueue;
    ComPtr<ID3D12CommandQueue> m_ComputeQueue;
    ComPtr<ID3D12CommandQueue> m_GraphicsQueue;
    ComPtr<ID3D12RootSignature> m_GlobalRootSignature;

    Mutex m_UploadMutex;
    DescriptorHeap m_ClearHeap;
    uint64_t m_UploadBuffersSize = 0;
    ComPtr<ID3D12Fence1> m_UploadFence;
    Array<UploadBuffer> m_UploadBuffers;
    Array<BufferUpload> m_BufferUploads;
    Array<TextureUpload> m_TextureUploads;
    Array<DeviceResource> m_DeferredReleaseQueue;
    StaticArray<DescriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> m_Heaps;
    StaticArray<ComPtr<ID3D12CommandSignature>, COMMAND_SIGNATURE_COUNT> m_CommandSignatures;

private:
    Buffer::Pool m_Buffers;
    Texture::Pool m_Textures;
};


class RingAllocator
{
public:
    void CreateBuffer(Device& inDevice, uint32_t inCapacity, uint32_t inAlignment);
    void DestroyBuffer(Device& inDevice);
    /*
        Allocates memory and memcpy's inData to the mapped buffer. ioOffset contains the offset from the starting pointer.
        This function default aligns to 4, so the offset can be used with HLSL byte address buffers directly:
        ByteAddressBuffer buffer;
        T data = buffer.Load<T>(ioOffset);
    */
    uint32_t AllocAndCopy(uint32_t inSize, const void* inData);

    template<typename T>
    uint32_t AllocAndCopy(const T& inStruct)
    {
        return AllocAndCopy(sizeof(T), (void*)&inStruct);
    }

    uint32_t GetOffset() const { return m_Offset; }
    BufferID GetBuffer() const { return m_Buffer; }

private:
    BufferID m_Buffer;
    uint32_t m_Size = 0;
    uint32_t m_Offset = 0;
    uint32_t m_Alignment = 0;
    uint8_t* m_DataPtr = nullptr;
    uint32_t m_TotalCapacity = 0;
};


class GlobalConstantsAllocator
{
public:
    void CreateBuffer(Device& inDevice);
    void DestroyBuffer(Device& inDevice);

    void Copy(const GlobalConstants& inGlobalConstants)
    {
        memcpy(m_DataPtr, &inGlobalConstants, sizeof(GlobalConstants));
    }

    BufferID GetBuffer() const { return m_Buffer; }

private:
    BufferID m_Buffer;
    uint8_t* m_DataPtr = nullptr;
};

} // namespace::Raekor

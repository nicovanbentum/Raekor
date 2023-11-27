#pragma once

#include "shared.h"
#include "DXResource.h"

namespace Raekor::DX12 {

class CommandList;
class IRenderPass;
class ShaderProgram;

class Device
{
public:
    NO_COPY_NO_MOVE(Device);

    Device(SDL_Window* window, uint32_t inFrameCount);

    ID3D12Device5* operator* ()              { return m_Device.Get(); }
    const ID3D12Device5* operator* () const  { return m_Device.Get(); }
    ID3D12Device5* operator-> ()             { return m_Device.Get(); }
    const ID3D12Device5* operator-> () const { return m_Device.Get(); }

    bool IsDLSSSupported() const { return mIsDLSSSupported; }
    bool IsTearingSupported() const { return mIsTearingSupported; }

    [[nodiscard]] ID3D12RootSignature* GetGlobalRootSignature() const { return m_GlobalRootSignature.Get(); }
    [[nodiscard]] DescriptorHeap& GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE inType) { return m_Heaps[inType]; }

    [[nodiscard]] ID3D12CommandQueue* GetCopyQueue() { return m_CopyQueue.Get(); }
    [[nodiscard]] ID3D12CommandQueue* GetComputeQueue() { return m_ComputeQueue.Get(); }
    [[nodiscard]] ID3D12CommandQueue* GetGraphicsQueue() { return m_GraphicsQueue.Get(); }

    void BindDrawDefaults(CommandList& inCmdList);
    void Submit(const Slice<CommandList>& inCmdLists);

    [[nodiscard]] BufferID  CreateBuffer(const Buffer::Desc& inDesc);
    [[nodiscard]] TextureID CreateTexture(const Texture::Desc& inDesc);

    [[nodiscard]] BufferID  CreateBufferView(BufferID inTextureID, const Buffer::Desc& inDesc);
    [[nodiscard]] TextureID CreateTextureView(TextureID inTextureID, const Texture::Desc& inDesc);
    [[nodiscard]] TextureID CreateTextureView(D3D12ResourceRef inResource, const Texture::Desc& inDesc);

    /* USE WITH CAUTION. NEXT CREATE* CALL WILL DELETE THE OLD RESOURCE. ONLY USE WHEN YOU KNOW THE GPU IS NO LONGER USING THE RESOURCE!! */
    void ReleaseBuffer(BufferID inID);
    /* USE WITH CAUTION. NEXT CREATE* CALL WILL DELETE THE OLD RESOURCE. ONLY USE WHEN YOU KNOW THE GPU IS NO LONGER USING THE RESOURCE!! */
    void ReleaseTexture(TextureID inID);

    /* USE WITH CAUTION. WILL IMMEDIATELY DELETE THE RESOURCE. ONLY USE WHEN YOU KNOW THE GPU IS NO LONGER USING THE RESOURCE!! */
    void ReleaseBufferImmediate(BufferID inID);
    /* USE WITH CAUTION. WILL IMMEDIATELY DELETE THE RESOURCE. ONLY USE WHEN YOU KNOW THE GPU IS NO LONGER USING THE RESOURCE!! */
    void ReleaseTextureImmediate(TextureID inID);

    [[nodiscard]] Buffer& GetBuffer(BufferID inID) { assert(inID.IsValid()); return m_Buffers.Get(inID); }
    [[nodiscard]] const Buffer& GetBuffer(BufferID inID) const { assert(inID.IsValid()); return m_Buffers.Get(inID); }

    [[nodiscard]] Texture& GetTexture(TextureID inID) { assert(inID.IsValid()); return m_Textures.Get(inID); }
    [[nodiscard]] const Texture& GetTexture(TextureID inID) const { assert(inID.IsValid()); return m_Textures.Get(inID); }

    [[nodiscard]] ID3D12Resource* GetD3D12Resource(BufferID inID) { return GetBuffer(inID).GetD3D12Resource().Get(); }
    [[nodiscard]] ID3D12Resource* GetD3D12Resource(TextureID inID) { return GetTexture(inID).GetD3D12Resource().Get(); }

    [[nodiscard]] const ID3D12Resource* GetD3D12Resource(BufferID inID) const { return GetBuffer(inID).GetD3D12Resource().Get(); }
    [[nodiscard]] const ID3D12Resource* GetD3D12Resource(TextureID inID) const { return GetTexture(inID).GetD3D12Resource().Get(); }

    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(BufferID inID);
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(TextureID inID);

    [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(BufferID inID);
    [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(TextureID inID);

    [[nodiscard]] DescriptorID CreateDepthStencilView(D3D12ResourceRef inD3D12Resource, const D3D12_DEPTH_STENCIL_VIEW_DESC* inDesc = nullptr);
    [[nodiscard]] DescriptorID CreateRenderTargetView(D3D12ResourceRef inD3D12Resource, const D3D12_RENDER_TARGET_VIEW_DESC* inDesc = nullptr);
    [[nodiscard]] DescriptorID CreateShaderResourceView(D3D12ResourceRef inD3D12Resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* inDesc = nullptr);
    [[nodiscard]] DescriptorID CreateUnorderedAccessView(D3D12ResourceRef inD3D12Resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* inDesc = nullptr);

    void ReleaseDepthStencilView(DescriptorID inDescriptorID) { m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Remove(inDescriptorID); }
    void ReleaseRenderTargetView(DescriptorID inDescriptorID) { m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Remove(inDescriptorID); }
    void ReleaseShaderResourceView(DescriptorID inDescriptorID) { m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Remove(inDescriptorID); }
    void ReleaseUnorderedAccessView(DescriptorID inDescriptorID) { m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Remove(inDescriptorID); }

    [[nodiscard]] D3D12_COMPUTE_PIPELINE_STATE_DESC  CreatePipelineStateDesc(IRenderPass* inRenderPass, const CD3DX12_SHADER_BYTECODE& inComputeShader);
    [[nodiscard]] D3D12_GRAPHICS_PIPELINE_STATE_DESC CreatePipelineStateDesc(IRenderPass* inRenderPass, const CD3DX12_SHADER_BYTECODE& inVertexShader, const CD3DX12_SHADER_BYTECODE& inPixelShader);

    [[nodiscard]] uint32_t GetBindlessHeapIndex(DescriptorID inID) { return inID.ToIndex(); }
    [[nodiscard]] uint32_t GetBindlessHeapIndex(BufferID inID) { return GetBindlessHeapIndex(GetBuffer(inID).GetDescriptor()); }
    [[nodiscard]] uint32_t GetBindlessHeapIndex(TextureID inID) { return GetBindlessHeapIndex(GetTexture(inID).GetView()); }

    IDXGIAdapter1* GetAdapter() { return m_Adapter.Get(); }

    const Buffer::Pool& GetBufferPool() const { return m_Buffers; }
    const Texture::Pool& GetTexturePool() const { return m_Textures; }

private:
    void CreateDescriptor(BufferID inBufferID, const Buffer::Desc& inDesc);
    void CreateDescriptor(TextureID inTextureID, const Texture::Desc& inDesc);
    void ReleaseDescriptor(BufferID inBufferID);
    void ReleaseDescriptor(TextureID inTextureID);

    /* USE WITH CAUTION. ONLY USE WHEN YOU KNOW THE GPU IS NO LONGER USING THE RESOURCE!! */
    void ReleaseDescriptorImmediate(BufferID inBufferID);
    /* USE WITH CAUTION. ONLY USE WHEN YOU KNOW THE GPU IS NO LONGER USING THE RESOURCE!! */
    void ReleaseDescriptorImmediate(TextureID inTextureID);

private:
    BOOL mIsDLSSSupported = false;
    BOOL mIsTearingSupported = false;
    ComPtr<ID3D12Device5> m_Device;
    ComPtr<IDXGIAdapter1> m_Adapter;
    ComPtr<ID3D12CommandQueue> m_CopyQueue;
    ComPtr<ID3D12CommandQueue> m_ComputeQueue;
    ComPtr<ID3D12CommandQueue> m_GraphicsQueue;
    ComPtr<D3D12MA::Allocator> m_Allocator;
    ComPtr<ID3D12RootSignature> m_GlobalRootSignature;
    std::array<DescriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> m_Heaps;

private:
    uint32_t m_NumFrames;
    Buffer::Pool m_Buffers;
    Texture::Pool m_Textures;
};


class StagingHeap
{
private:
    struct StagingBuffer
    {
        bool mRetired = true;
        uint32_t mFrameIndex = 0;
        BufferID mBufferID = BufferID();
        size_t mSize = 0;
        size_t mCapacity = 0;
        uint8_t* mPtr = nullptr;
    };

public:
    StagingHeap(Device& inDevice) : m_Device(inDevice) {}
    ~StagingHeap();

    void StageBuffer(CommandList& inCmdList, const Buffer& inBuffer, uint32_t inOffset, const void* inData, uint32_t inSize);
    void StageTexture(CommandList& inCmdList, const Texture& inTexture, uint32_t inSubResource, const void* inData);
    void RetireBuffers(CommandList& inCmdList);

private:
    Device& m_Device;
    std::vector<StagingBuffer> m_Buffers;
};


class RingAllocator
{
public:
    void CreateBuffer(Device& inDevice, uint32_t inCapacity);
    void DestroyBuffer(Device& inDevice);

    /*
        Allocates memory for inStruct and memcpy's it to the mapped buffer. ioOffset contains the offset from the starting pointer.
        This function default aligns to 4, so the offset can be used with HLSL byte address buffers directly:
        ByteAddressBuffer buffer;
        T data = buffer.Load<T>(ioOffset);
    */
    template<typename T>
    void AllocAndCopy(const T& inStruct, uint32_t& ioOffset, uint32_t inAlignment = sByteAddressBufferAlignment)
    {
        const auto size = gAlignUp(sizeof(T), inAlignment);
        assert(m_Size + size <= m_TotalCapacity);

        memcpy(m_DataPtr + m_Size, &inStruct, sizeof(T));
        ioOffset = m_Size;

        m_Size += size;

        if (m_Size == m_TotalCapacity)
            m_Size = 0;
    }

    inline BufferID GetBuffer() const { return m_Buffer; }

private:
    BufferID m_Buffer;
    uint32_t m_Size = 0;
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

    inline BufferID GetBuffer() const { return m_Buffer; }

private:
    BufferID m_Buffer;
    uint8_t* m_DataPtr = nullptr;
};

} // namespace::Raekor

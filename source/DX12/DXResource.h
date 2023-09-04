#pragma once

#include "DXUtil.h"

namespace Raekor::DX12 {

using ResourceRef = ComPtr<ID3D12Resource>;
using AllocationRef = ComPtr<D3D12MA::Allocation>;

using DescriptorPool = FreeVector<ResourceRef>;
using DescriptorID = DescriptorPool::ID;

class Texture
{
    friend class Device;
    friend class CommandList;

public:
    using Pool = FreeVector<Texture>;

    enum Usage
    {
        GENERAL,
        SHADER_READ_ONLY,
        SHADER_READ_WRITE,
        RENDER_TARGET,
        DEPTH_STENCIL_TARGET
    };

    struct Desc
    {
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t depth = 1;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
        bool shaderAccess = false;
        bool mappable = false;
        Usage usage = Usage::GENERAL;
        void* viewDesc = nullptr;
    };

    Texture() = default;
    Texture(const Desc& inDesc) : m_Desc(inDesc) {}

    const Desc& GetDesc() const { return m_Desc; }
    DescriptorID GetView() const { return m_Descriptor; }
    uint32_t GetHeapIndex() const { return m_Descriptor.ToIndex(); }

    ResourceRef& operator-> () { return m_Resource; }
    const ResourceRef& operator-> () const { return m_Resource; }
    ResourceRef& GetResource() { return m_Resource; }
    const ResourceRef& GetResource() const { return m_Resource; }

private:
    Desc m_Desc = {};
    DescriptorID m_Descriptor;
    ResourceRef m_Resource = nullptr;
    AllocationRef m_Allocation = nullptr;
};


class Buffer
{
    friend class Device;
    friend class CommandList;

public:
    using Pool = FreeVector<Buffer>;

    enum Usage
    {
        UPLOAD = 1 << 0,
        GENERAL = 1 << 1,
        INDEX_BUFFER = 1 << 2,
        VERTEX_BUFFER = 1 << 3,
        SHADER_READ_ONLY = 1 << 4,
        SHADER_READ_WRITE = 1 << 5,
        ACCELERATION_STRUCTURE = 1 << 6
    };

    struct Desc
    {
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        uint64_t size = 0;
        uint64_t stride = 0;
        Usage usage = Usage::GENERAL;
        bool mappable = false;
        void* viewDesc = nullptr;
    };

    Buffer() = default;
    Buffer(const Desc& inDesc) : m_Desc(inDesc) {}
    ~Buffer() { if (m_MappedPtr) m_Resource->Unmap(0, nullptr); }

    const Desc& GetDesc() const { return m_Desc; }
    DescriptorID GetDescriptor() const { return m_Descriptor; }
    uint32_t GetHeapIndex() const { return m_Descriptor.ToIndex(); }

    ResourceRef& operator-> () { return m_Resource; }
    const ResourceRef& operator-> () const { return m_Resource; }
    ResourceRef& GetResource() { return m_Resource; }
    const ResourceRef& GetResource() const { return m_Resource; }

private:
    Desc m_Desc = {};
    void* m_MappedPtr = nullptr;
    DescriptorID m_Descriptor;
    ResourceRef m_Resource = nullptr;
    AllocationRef m_Allocation = nullptr;
};


using BufferID = Buffer::Pool::ID;
using TextureID = Texture::Pool::ID;


D3D12_RESOURCE_STATES gGetResourceStates(Buffer::Usage inUsage);
D3D12_RESOURCE_STATES gGetResourceStates(Texture::Usage inUsage);


D3D12_DESCRIPTOR_HEAP_TYPE gGetHeapType(Buffer::Usage inUsage);
D3D12_DESCRIPTOR_HEAP_TYPE gGetHeapType(Texture::Usage inUsage);


class DescriptorHeap : public DescriptorPool
{
public:
    void Init(ID3D12Device* inDevice, D3D12_DESCRIPTOR_HEAP_TYPE inType, uint32_t inCount, D3D12_DESCRIPTOR_HEAP_FLAGS inFlags);

    ID3D12DescriptorHeap* GetHeap() const { return m_Heap.Get(); }

    inline D3D12_CPU_DESCRIPTOR_HANDLE  GetCPUDescriptorHandle(ID inResourceID) const
    {
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_HeapPtr, inResourceID.ToIndex(), m_HeapIncrement);
    }

    inline D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID inResourceID) const
    {
        return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_Heap->GetGPUDescriptorHandleForHeapStart(), inResourceID.ToIndex(), m_HeapIncrement);
    }

private:
    UINT m_HeapIncrement = 0;
    ComPtr<ID3D12DescriptorHeap> m_Heap = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE m_HeapPtr = { .ptr = 0 };
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace::Raekor
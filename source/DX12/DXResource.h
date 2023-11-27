#pragma once

#include "DXUtil.h"

namespace Raekor::DX12 {

class Device;

class ResourceID
{
public:
    static inline uint32_t INVALID = 0xFFFFF;

    ResourceID() : m_Index(INVALID) {}
    explicit ResourceID(uint32_t inValue) : m_Index(inValue) {}
    explicit operator uint32_t() const { return m_Value; }

    bool operator==(const ResourceID& inOther) const { return ToIndex() == inOther.ToIndex(); }
    bool operator!=(const ResourceID& inOther) const { return ToIndex() != inOther.ToIndex(); }

    inline uint32_t ToIndex() const { return m_Index; }
    [[nodiscard]] bool IsValid() const { return m_Index != INVALID; }

protected:
    union
    {
        struct
        {
            uint32_t m_Index : 20;
            uint32_t m_Generation : 12;
        };

        uint32_t m_Value;
    };
};


template<typename T>
class TypedResourceID : public ResourceID 
{
public:
    TypedResourceID() = default;
    TypedResourceID(uint32_t inValue) : ResourceID(inValue) {}
    TypedResourceID(ResourceID inValue) : ResourceID(inValue) {}
};


template<typename T>
class ResourcePool
{
public:
    using TypedID = TypedResourceID<T>;
    virtual ~ResourcePool() = default;

    void Reserve(size_t inSizeInBytes)
    {
        m_Storage.reserve(inSizeInBytes);
    }

    [[nodiscard]] virtual TypedID Add(const T& inT)
    {
        size_t t_index;

        if (m_FreeIndices.empty())
        {
            m_Storage.emplace_back(inT);
            t_index = m_Storage.size() - 1;
        }
        else
        {
            t_index = m_FreeIndices.back();
            m_Storage[t_index] = inT;
            m_FreeIndices.pop_back();
        }

        return TypedID(t_index);
    }

    bool Remove(TypedID inID)
    {
        if (inID.ToIndex() > m_Storage.size() - 1)
            return false;

        for (auto free_index : m_FreeIndices)
            if (free_index == inID.ToIndex())
                return false;

        m_FreeIndices.push_back(inID.ToIndex());
        return true;
    }

    T& Get(TypedID inRTID)
    {
        assert(!m_Storage.empty() && inRTID.ToIndex() < m_Storage.size());
        return m_Storage[inRTID.ToIndex()];
    }

    const T& Get(TypedID inRTID) const
    {
        assert(!m_Storage.empty() && inRTID.ToIndex() < m_Storage.size());
        return m_Storage[inRTID.ToIndex()];
    }

    size_t GetSize() const 
    { 
        return m_Storage.size(); 
    }

private:
    std::vector<uint16_t> m_Generations;
    std::vector<uint32_t> m_FreeIndices;
    std::vector<T> m_Storage;
};


using D3D12ResourceRef = ComPtr<ID3D12Resource>;
using D3D12AllocationRef = ComPtr<D3D12MA::Allocation>;


using DescriptorPool = ResourcePool<D3D12ResourceRef>;
using DescriptorID = DescriptorPool::TypedID;



class DeviceResource
{
public:
    friend class Device;

    DeviceResource() = default;
    DeviceResource(const D3D12ResourceRef& m_Resource, const D3D12AllocationRef& m_Allocation)
        : m_Resource(m_Resource), m_Allocation(m_Allocation) {}
    virtual ~DeviceResource() = default;

    D3D12ResourceRef& operator-> () { return m_Resource; }
    const D3D12ResourceRef& operator-> () const { return m_Resource; }
    D3D12ResourceRef& GetD3D12Resource() { return m_Resource; }
    const D3D12ResourceRef& GetD3D12Resource() const { return m_Resource; }
    D3D12AllocationRef& GetD3D12Allocation() { return m_Allocation; }
    const D3D12AllocationRef& GetD3D12Allocation() const { return m_Allocation; }

protected:
    D3D12ResourceRef m_Resource = nullptr;
    D3D12AllocationRef m_Allocation = nullptr;
};



class Texture : public DeviceResource
{
    friend class Device;
    friend class CommandList;

public:
    using Pool = ResourcePool<Texture>;

    enum Usage
    {
        GENERAL,
        SHADER_READ_ONLY,
        SHADER_READ_WRITE,
        RENDER_TARGET,
        DEPTH_STENCIL_TARGET
    };

    enum Dimension
    {
        TEX_DIM_2D,
        TEX_DIM_3D,
        TEX_DIM_CUBE
    };

    struct Desc
    {
        uint8_t swizzle = TEXTURE_SWIZZLE_RGBA;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        Dimension dimension = TEX_DIM_2D;
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t depth = 1;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
        bool shaderAccess = false;
        bool mappable = false;
        Usage usage = Usage::GENERAL;
        void* viewDesc = nullptr;
        const wchar_t* debugName = nullptr;
    };

    Texture() = default;
    Texture(const Desc& inDesc) : m_Desc(inDesc) {}
    explicit Texture(const Texture& inRHS) = default;

    const Desc& GetDesc() const { return m_Desc; }
    DescriptorID GetView() const { return m_Descriptor; }
    uint32_t GetHeapIndex() const { return m_Descriptor.ToIndex(); }

private:
    Desc m_Desc = {};
    DescriptorID m_Descriptor;
};


class Buffer : public DeviceResource
{
    friend class Device;
    friend class CommandList;

public:
    using Pool = ResourcePool<Buffer>;

    enum Usage
    {
        GENERAL                 = 1 << 1,
        UPLOAD                  = 1 << 2,
        READBACK                = 1 << 3,
        INDEX_BUFFER            = 1 << 4,
        VERTEX_BUFFER           = 1 << 5,
        SHADER_READ_ONLY        = 1 << 6,
        SHADER_READ_WRITE       = 1 << 7,
        ACCELERATION_STRUCTURE  = 1 << 8
    };

    struct Desc
    {
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        uint64_t size = 0;
        uint64_t stride = 0;
        Usage usage = Usage::GENERAL;
        bool mappable = false;
        void* viewDesc = nullptr;
        const wchar_t* debugName = nullptr;
    };

    Buffer() = default;
    Buffer(const Desc& inDesc) : m_Desc(inDesc) {}
    ~Buffer() { if (m_MappedPtr) m_Resource->Unmap(0, nullptr); }

    const Desc& GetDesc() const { return m_Desc; }
    const uint32_t GetSize() const { return m_Desc.size; }

    DescriptorID GetDescriptor() const { return m_Descriptor; }
    uint32_t GetHeapIndex() const { return m_Descriptor.ToIndex(); }

private:
    Desc m_Desc = {};
    DescriptorID m_Descriptor;
    void* m_MappedPtr = nullptr;
};


using BufferID = Buffer::Pool::TypedID;
using TextureID = Texture::Pool::TypedID;


struct BufferUpload
{
    BufferID mBuffer;
    Slice<char> mData;
};

struct TextureUpload
{
    uint32_t mMip = 0;
    TextureID mTexture;
    Slice<char> mData;
};


enum EResourceType
{
    RESOURCE_TYPE_BUFFER,
    RESOURCE_TYPE_TEXTURE
};


D3D12_RESOURCE_STATES gGetResourceStates(Buffer::Usage inUsage);
D3D12_RESOURCE_STATES gGetResourceStates(Texture::Usage inUsage);


D3D12_DESCRIPTOR_HEAP_TYPE gGetHeapType(Buffer::Usage inUsage);
D3D12_DESCRIPTOR_HEAP_TYPE gGetHeapType(Texture::Usage inUsage);


class DescriptorHeap : public DescriptorPool
{
private:
    friend class Device; // Only Device is allowed to create DescriptorHeap's
    DescriptorHeap(Device& inDevice, D3D12_DESCRIPTOR_HEAP_TYPE inType, uint32_t inCount, D3D12_DESCRIPTOR_HEAP_FLAGS inFlags);

public:
    DescriptorHeap() = default;

    ID3D12DescriptorHeap* operator* ()              { return m_Heap.Get(); }
    const ID3D12DescriptorHeap* operator* () const  { return m_Heap.Get(); }
    ID3D12DescriptorHeap* operator-> ()             { return m_Heap.Get(); }
    const ID3D12DescriptorHeap* operator-> () const { return m_Heap.Get(); }

    inline D3D12_CPU_DESCRIPTOR_HANDLE  GetCPUDescriptorHandle(TypedID inResourceID) const
    {
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_HeapPtr, inResourceID.ToIndex(), m_HeapIncrement);
    }

    inline D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(TypedID inResourceID) const
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

template<typename T> struct std::hash<Raekor::DX12::TypedResourceID<T>>
{
    std::size_t operator()(const Raekor::DX12::TypedResourceID<T>& inID) const noexcept
    {
        return size_t(inID.ToIndex());
    }
};
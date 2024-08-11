#pragma once

#include "RenderUtil.h"
#include "Defines.h"

namespace RK::DX12 {

class Device;

class ResourceID
{
public:
    static inline uint32_t cInvalidIndex = 0xFFFFF;

    ResourceID() : m_Index(cInvalidIndex), m_Generation(0) {}
    explicit ResourceID(uint32_t inValue) : m_Value(inValue) {}

    explicit operator uint32_t() const { return m_Value; }

    bool operator==(const ResourceID& inOther) const { return m_Index == inOther.m_Index; }
    bool operator!=(const ResourceID& inOther) const { return m_Index != inOther.m_Index; }

    inline uint32_t GetIndex() const { return m_Index; }
    inline uint32_t GetValue() const { return m_Value; }
    inline uint32_t GetGeneration() const { return m_Generation; }

    [[nodiscard]] bool IsValid() const { return m_Index != cInvalidIndex; }

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

    void Reserve(size_t inSize) 
    { 
        m_Storage.reserve(inSize);
        m_Generations.reserve(inSize);
        m_FreeIndices.reserve(inSize);
    }

    [[nodiscard]] TypedID Add(const T& inT)
    {
        TypedID id;

        if (m_FreeIndices.empty())
        {
            m_Storage.emplace_back(inT);
            m_Generations.emplace_back(0);

            id.m_Index = m_Storage.size() - 1;
            id.m_Generation = 0;
        }
        else
        {
            id.m_Index = m_FreeIndices.back();
            id.m_Generation = m_Generations[id.m_Index];

            m_Storage[id.m_Index] = inT;
            m_FreeIndices.pop_back();
        }

        return id;
    }

    void Remove(TypedID inID)
    {
        assert(inID.IsValid());
        assert(inID.m_Index < m_Storage.size());

        m_Generations[inID.GetIndex()]++;
        m_FreeIndices.push_back(inID.GetIndex());
    }

    T& Get(TypedID inID)
    {
        assert(!m_Storage.empty() && inID.GetIndex() < m_Storage.size());
        assert(inID.m_Generation == m_Generations[inID.GetIndex()]);
        return m_Storage[inID.GetIndex()];
    }

    const T& Get(TypedID inID) const
    {
        assert(!m_Storage.empty() && inID.GetIndex() < m_Storage.size());
        assert(inID.m_Generation == m_Generations[inID.GetIndex()]);
        return m_Storage[inID.GetIndex()];
    }

    inline size_t GetSize() const { return m_Storage.size(); }

    void Clear() 
    {
        m_Storage.clear();
        m_Generations.clear();
        m_FreeIndices.clear();
    }

    auto begin() { return m_Storage.begin(); }
    auto end() { return m_Storage.end(); }

    auto begin() const { return m_Storage.begin(); }
    auto end() const { return m_Storage.end(); }

protected:
    Array<uint16_t> m_Generations;
    Array<uint32_t> m_FreeIndices;
    Array<T> m_Storage;
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
        uint8_t swizzle             = TEXTURE_SWIZZLE_RGBA;
        DXGI_FORMAT format          = DXGI_FORMAT_UNKNOWN;
        Dimension dimension         = TEX_DIM_2D;
        uint32_t width              = 1;
        uint32_t height             = 1;
        uint32_t depthOrArrayLayers = 1;
        uint32_t baseMip            = 0;
        uint32_t mipLevels          = 1;
        Usage usage                 = Usage::GENERAL;
        bool __allowDepthTarget     = false; // only used by the render graph
        bool __allowRenderTarget    = false; // only used by the render graph
        bool __allowUnorderedAccess = false; // only used by the render graph
        const char* debugName       = nullptr; // SHOULD ALWAYS BE THE LAST MEMBER!!

        inline bool operator==(const Desc& inOther) const { return std::memcmp(this, &inOther, offsetof(Desc, debugName)) == 0; }

        D3D12_DEPTH_STENCIL_VIEW_DESC    ToDSVDesc() const;
        D3D12_RENDER_TARGET_VIEW_DESC    ToRTVDesc() const;
        D3D12_SHADER_RESOURCE_VIEW_DESC  ToSRVDesc() const;
        D3D12_UNORDERED_ACCESS_VIEW_DESC ToUAVDesc() const;

        D3D12_RESOURCE_DESC ToResourceDesc() const;
        D3D12MA::ALLOCATION_DESC ToAllocationDesc() const;
    };

    static Desc Desc2D(DXGI_FORMAT inFormat, uint32_t inWidth, uint32_t inHeight, Usage inUsage, uint32_t inBaseMip = 0, uint32_t inMipLevels = 1)
    {
        return Desc { .format = inFormat, .dimension = TEX_DIM_2D, .width = inWidth, .height = inHeight, .baseMip = inBaseMip, .mipLevels = inMipLevels, .usage = inUsage, };
    }

    static Desc DescCube(DXGI_FORMAT inFormat, uint32_t inWidth, uint32_t inHeight, Usage inUsage, uint32_t inBaseMip = 0, uint32_t inMipLevels = 1)
    {
        return Desc { .format = inFormat, .dimension = TEX_DIM_CUBE, .width = inWidth, .height = inHeight, .depthOrArrayLayers = 6, .baseMip = inBaseMip, .mipLevels = inMipLevels, .usage = inUsage, };
    }

    Texture() = default;
    Texture(const Desc& inDesc) : m_Desc(inDesc) {}
    explicit Texture(const Texture& inRHS) = default;

    const Desc& GetDesc() const { return m_Desc; }
    DescriptorID GetView() const { return m_Descriptor; }

    Usage GetUsage() const { return m_Desc.usage; }
    uint32_t GetWidth() const { return m_Desc.width; }
    uint32_t GetHeight() const { return m_Desc.height; }
    DXGI_FORMAT GetFormat() const { return m_Desc.format; }
    
    bool HasDescriptor() const { return m_Descriptor.IsValid(); }
    DescriptorID GetDescriptor() const { return m_Descriptor; }
    
    uint32_t GetHeapIndex() const { return m_Descriptor.GetIndex(); }

    uint32_t GetBaseSubresource() const { return m_Desc.baseMip; }
    uint32_t GetSubresourceCount() const { return m_Desc.mipLevels * m_Desc.depthOrArrayLayers; }

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
        GENERAL,
        UPLOAD,
        READBACK,
        INDEX_BUFFER,
        VERTEX_BUFFER,
        SHADER_READ_ONLY,
        SHADER_READ_WRITE,
        INDIRECT_ARGUMENTS,
        ACCELERATION_STRUCTURE
    };

    enum class ShaderUsage : int
    {
        READ_ONLY = Usage::SHADER_READ_ONLY,
        READ_WRITE = Usage::SHADER_READ_WRITE
    };

    struct Desc
    {
        DXGI_FORMAT format          = DXGI_FORMAT_UNKNOWN;
        uint64_t size               = 0;
        uint64_t stride             = 0;
        Usage usage                 = Usage::GENERAL;
        bool mappable               = false;
        const char* debugName       = nullptr;

        inline bool operator==(const Desc& inOther) const { return std::memcmp(this, &inOther, offsetof(Desc, debugName)) == 0; }

        D3D12_SHADER_RESOURCE_VIEW_DESC  ToSRVDesc() const;
        D3D12_UNORDERED_ACCESS_VIEW_DESC ToUAVDesc() const;

        D3D12_RESOURCE_DESC ToResourceDesc() const;
        D3D12MA::ALLOCATION_DESC ToAllocationDesc() const;
    };

    static Desc Describe(uint64_t inSize, Usage inUsage, bool inMappable = false, const char* inDebugName = nullptr)
    {
        return  Desc { .size = inSize, .usage = inUsage, .mappable = inMappable, .debugName = inDebugName };
    }

    static Desc Describe(DXGI_FORMAT inFormat, uint64_t inSize, Usage inUsage, uint64_t inStride = 0, bool inMappable = false, const char* inDebugName = nullptr)
    { 
        return  Desc { .format = inFormat, .size = inSize, .stride = inStride, .usage = inUsage, .mappable = inMappable, .debugName = inDebugName };
    }

    static Desc TypedBuffer(DXGI_FORMAT inFormat, uint64_t inSize, const char* inDebugName = nullptr)
    {
        return  Desc { .format = inFormat, .size = inSize, .stride = 0, .usage = SHADER_READ_ONLY, .mappable = false, .debugName = inDebugName };
    }

    static Desc RWTypedBuffer(DXGI_FORMAT inFormat, uint64_t inSize, const char* inDebugName = nullptr)
    {
        return  Desc { .format = inFormat, .size = inSize, .stride = 0, .usage = SHADER_READ_WRITE, .mappable = false, .debugName = inDebugName };
    }

    static Desc ByteAddressBuffer(uint64_t inSize, const char* inDebugName = nullptr)
    {
        return  Desc { .format = DXGI_FORMAT_R32_TYPELESS, .size = inSize, .stride = 0, .usage = SHADER_READ_ONLY, .mappable = false, .debugName = inDebugName };
    }

    static Desc RWByteAddressBuffer(uint64_t inSize, const char* inDebugName = nullptr)
    {
        return  Desc { .format = DXGI_FORMAT_R32_TYPELESS, .size = inSize, .stride = 0, .usage = SHADER_READ_WRITE, .mappable = false, .debugName = inDebugName };
    }

    static Desc StructuredBuffer(uint64_t inSize, uint64_t inStride, const char* inDebugName = nullptr)
    {
        return  Desc { .format = DXGI_FORMAT_UNKNOWN, .size = inSize, .stride = inStride, .usage = SHADER_READ_ONLY, .mappable = false, .debugName = inDebugName };
    }

    static Desc RWStructuredBuffer(uint64_t inSize, uint64_t inStride, const char* inDebugName = nullptr)
    {
        return  Desc { .format = DXGI_FORMAT_UNKNOWN, .size = inSize, .stride = inStride, .usage = SHADER_READ_WRITE, .mappable = false, .debugName = inDebugName };
    }

    Buffer() = default;
    Buffer(const Desc& inDesc) : m_Desc(inDesc) {}
    ~Buffer() { if (m_MappedPtr) m_Resource->Unmap(0, nullptr); }

    const Desc& GetDesc() const { return m_Desc; }
    
    Usage GetUsage() const { return m_Desc.usage; }
    uint32_t GetSize() const { return m_Desc.size; }
    DXGI_FORMAT GetFormat() const { return m_Desc.format; }

    bool HasDescriptor() const { return m_Descriptor.IsValid(); }
    DescriptorID GetDescriptor() const { return m_Descriptor; }

    uint32_t GetHeapIndex() const { return m_Descriptor.GetIndex(); }

    uint32_t GetBaseSubresource() const { return 0; }
    uint32_t GetSubresourceCount() const { return 1; }

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
    ByteSlice mData;
};

struct TextureUpload
{
    uint32_t mMip = 0;
    TextureID mTexture;
    ByteSlice mData;
};

struct UploadBuffer
{
    bool mRetired        = true;
    uint32_t mFrameIndex = 0;
    size_t mSize         = 0;
    size_t mCapacity     = 0;
    uint8_t* mPtr        = nullptr;
    BufferID mID         = BufferID();
};


D3D12_RESOURCE_STATES GetD3D12ResourceStates(Buffer::Usage inUsage);
D3D12_RESOURCE_STATES GetD3D12ResourceStates(Texture::Usage inUsage);

D3D12_RESOURCE_STATES GetD3D12InitialResourceStates(Buffer::Usage inUsage);
D3D12_RESOURCE_STATES GetD3D12InitialResourceStates(Texture::Usage inUsage);

D3D12_DESCRIPTOR_HEAP_TYPE GetD3D12HeapType(Buffer::Usage inUsage);
D3D12_DESCRIPTOR_HEAP_TYPE GetD3D12HeapType(Texture::Usage inUsage);


class DescriptorHeap : public DescriptorPool
{
public:
    ID3D12DescriptorHeap* operator* ()              { return m_Heap.Get(); }
    const ID3D12DescriptorHeap* operator* () const  { return m_Heap.Get(); }
    ID3D12DescriptorHeap* operator-> ()             { return m_Heap.Get(); }
    const ID3D12DescriptorHeap* operator-> () const { return m_Heap.Get(); }

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(DescriptorID inResourceID) const
    {
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_HeapPtr, inResourceID.GetIndex(), m_HeapIncrement);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(DescriptorID inResourceID) const
    {
        return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_Heap->GetGPUDescriptorHandleForHeapStart(), inResourceID.GetIndex(), m_HeapIncrement);
    }

private:
    friend class Device; // Only Device is allowed to create DescriptorHeap's
    void Allocate(Device& inDevice, D3D12_DESCRIPTOR_HEAP_TYPE inType, uint32_t inCount, D3D12_DESCRIPTOR_HEAP_FLAGS inFlags);

private:
    UINT m_HeapIncrement = 0;
    ComPtr<ID3D12DescriptorHeap> m_Heap = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE m_HeapPtr = { .ptr = 0 };
};


enum ECommandSignature
{
    COMMAND_SIGNATURE_DRAW       = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW,
    COMMAND_SIGNATURE_DRAW_INDEX = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED,
    COMMAND_SIGNATURE_DISPATCH   = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH,
    COMMAND_SIGNATURE_COUNT
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace::Raekor

template<>
struct std::hash<RK::DX12::ResourceID>
{
    std::size_t operator()(const RK::DX12::ResourceID& inID) const noexcept
    {
        return size_t(inID.GetValue());
    }
};

template<typename T> struct std::hash<RK::DX12::TypedResourceID<T>>
{
    std::size_t operator()(const RK::DX12::TypedResourceID<T>& inID) const noexcept
    {
        return size_t(inID.GetValue());
    }
};

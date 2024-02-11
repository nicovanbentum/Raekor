#include "pch.h"
#include "DXResource.h"
#include "DXDevice.h"

namespace Raekor::DX12 {

D3D12_RESOURCE_STATES gGetResourceStates(Buffer::Usage inUsage)
{
    switch (inUsage)
    {
        case Buffer::Usage::VERTEX_BUFFER:
            return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        case Buffer::Usage::INDEX_BUFFER:
            return D3D12_RESOURCE_STATE_INDEX_BUFFER;
        case Buffer::Usage::UPLOAD:
            return D3D12_RESOURCE_STATE_COMMON;
        case Buffer::Usage::SHADER_READ_ONLY:
            return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
        case Buffer::Usage::SHADER_READ_WRITE:
            return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        case Buffer::Usage::INDIRECT_ARGUMENTS:
            return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    }

    return D3D12_RESOURCE_STATES();
}


D3D12_RESOURCE_STATES gGetResourceStates(Texture::Usage inUsage)
{
    switch (inUsage)
    {
        case Texture::Usage::RENDER_TARGET:
            return D3D12_RESOURCE_STATE_RENDER_TARGET;
        case Texture::Usage::DEPTH_STENCIL_TARGET:
            return D3D12_RESOURCE_STATE_DEPTH_WRITE;
        case Texture::Usage::GENERAL:
            return D3D12_RESOURCE_STATE_COMMON;
        case Texture::Usage::SHADER_READ_ONLY:
            return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
        case Texture::Usage::SHADER_READ_WRITE:
            return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        default:
            assert(false);
    }

    return D3D12_RESOURCE_STATES();
}


D3D12_DESCRIPTOR_HEAP_TYPE gGetHeapType(Buffer::Usage inUsage)
{
    return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
}


D3D12_DESCRIPTOR_HEAP_TYPE gGetHeapType(Texture::Usage inUsage)
{
    switch (inUsage)
    {
        case Texture::DEPTH_STENCIL_TARGET:
            return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        case Texture::RENDER_TARGET:
            return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        case Texture::SHADER_READ_ONLY:
        case Texture::SHADER_READ_WRITE:
            return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        default:
            assert(false);
    }

    assert(false);
    return D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
}


D3D12_SHADER_RESOURCE_VIEW_DESC Buffer::Desc::ToSRVDesc() const
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = { .Format = format, .ViewDimension = D3D12_SRV_DIMENSION_BUFFER };
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    // Raw (ByteAddress) buffer
    if (stride == 0 && format == DXGI_FORMAT_R32_TYPELESS)
    {
        assert(size >= 4);
        srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        srv_desc.Buffer.NumElements = size / sizeof(uint32_t);
    }
    // Structured buffer
    else if (stride > 0 && format == DXGI_FORMAT_UNKNOWN) 
    {
        assert(size > 0);
        srv_desc.Buffer.StructureByteStride = stride;
        srv_desc.Buffer.NumElements = size / stride;
    }
    // Typed buffer
    else if (format != DXGI_FORMAT_UNKNOWN)
    {
        const auto format_size = gBitsPerPixel(srv_desc.Format);
        assert(size && format_size && format_size%8 == 0);
        srv_desc.Buffer.NumElements = size / ( format_size / 8 );
    }

    if (usage == Buffer::ACCELERATION_STRUCTURE)
    {
        srv_desc.Format = DXGI_FORMAT_UNKNOWN;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    }

    return srv_desc;
}


D3D12_UNORDERED_ACCESS_VIEW_DESC Buffer::Desc::ToUAVDesc() const
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = { .Format = format, .ViewDimension = D3D12_UAV_DIMENSION_BUFFER };

    // Raw buffer
    if (stride == 0 && format == DXGI_FORMAT_R32_TYPELESS)
    {
        assert(size >= 4);
        uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        uav_desc.Buffer.NumElements = size / sizeof(uint32_t);
    }
    // Structured buffer
    else if (stride > 0 && format == DXGI_FORMAT_UNKNOWN) 
    {
        assert(size > 0);
        uav_desc.Buffer.StructureByteStride = stride;
        uav_desc.Buffer.NumElements = size / stride;
    }
    // Typed buffer
    else if (format != DXGI_FORMAT_UNKNOWN)
    {
        const auto format_size = gBitsPerPixel(format);
        assert(size && format_size && format_size%8 == 0);
        uav_desc.Buffer.NumElements = size / ( format_size / 8 );
    }

    // defined both a stride and format, bad time!
    assert((stride > 0 && format != DXGI_FORMAT_UNKNOWN) == false);

    return uav_desc;
}


D3D12_DEPTH_STENCIL_VIEW_DESC Texture::Desc::ToDSVDesc() const 
{ 
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
    dsv_desc.Format = format;
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    if (dimension == Texture::Dimension::TEX_DIM_2D)
        dsv_desc.Texture2D.MipSlice = baseMip;

    return dsv_desc;
}


D3D12_RENDER_TARGET_VIEW_DESC Texture::Desc::ToRTVDesc() const 
{ 
    D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
    rtv_desc.Format = format;
    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    if (dimension == Texture::Dimension::TEX_DIM_2D)
        rtv_desc.Texture2D.MipSlice = baseMip;

    return rtv_desc;
}


D3D12_SHADER_RESOURCE_VIEW_DESC Texture::Desc::ToSRVDesc() const 
{ 
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = gGetDepthFormatSRV(format);
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = -1;

    const auto [r, g, b, a] = gUnswizzle(swizzle);
    srv_desc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(r, g, b, a);

    switch (dimension)
    {
        case Texture::TEX_DIM_CUBE:
        {
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srv_desc.TextureCube.MipLevels = -1;
            srv_desc.TextureCube.MostDetailedMip = 0;
            srv_desc.TextureCube.ResourceMinLODClamp = 0.0f;
        } break;
    }

    return srv_desc;
}


D3D12_UNORDERED_ACCESS_VIEW_DESC Texture::Desc::ToUAVDesc() const 
{ 
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
    uav_desc.Format = format;
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uav_desc.Texture2D.MipSlice = baseMip;

    return uav_desc;
}


void DescriptorHeap::Allocate(Device& inDevice, D3D12_DESCRIPTOR_HEAP_TYPE inType, uint32_t inCount, D3D12_DESCRIPTOR_HEAP_FLAGS inFlags)
{
    Reserve(inCount);

    constexpr auto heap_type_debug_names = std::array
    {
        L"CBV_SRV_UAV_HEAP",
        L"SAMPLER_HEAP",
        L"RTV_HEAP",
        L"DSV_HEAP"
    };

    const auto desc = D3D12_DESCRIPTOR_HEAP_DESC
    {
        .Type = inType,
        .NumDescriptors = inCount,
        .Flags = inFlags
    };

    gThrowIfFailed(inDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_Heap)));
    m_Heap->SetName(heap_type_debug_names[inType]);

    m_HeapPtr = m_Heap->GetCPUDescriptorHandleForHeapStart();
    m_HeapIncrement = inDevice->GetDescriptorHandleIncrementSize(inType);
}


} // namespace Raekor::DX12
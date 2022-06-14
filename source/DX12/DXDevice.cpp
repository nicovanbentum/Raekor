#include "pch.h"
#include "DXDevice.h"
#include "DXSampler.h"
#include "DXUtil.h"

namespace Raekor::DX {

Device::Device(SDL_Window* window) {
    UINT device_creation_flags = 0;

#ifndef NDEBUG
    ComPtr<ID3D12Debug1> mDebug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&mDebug)))) {
        mDebug->EnableDebugLayer();
    }

    device_creation_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    ComPtr<IDXGIFactory4> factory;
    gThrowIfFailed(CreateDXGIFactory2(device_creation_flags, IID_PPV_ARGS(&factory)));

    gThrowIfFailed(factory->EnumAdapters1(0, &m_Adapter));

    gThrowIfFailed(D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&m_Device)));

    D3D12MA::ALLOCATOR_DESC allocator_desc = {};
    allocator_desc.pAdapter = m_Adapter.Get();
    allocator_desc.pDevice = m_Device.Get();
    gThrowIfFailed(D3D12MA::CreateAllocator(&allocator_desc, &m_Allocator));

    D3D12_COMMAND_QUEUE_DESC qd = {};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    gThrowIfFailed(m_Device->CreateCommandQueue(&qd, IID_PPV_ARGS(&m_Queue)));

    qd.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    gThrowIfFailed(m_Device->CreateCommandQueue(&qd, IID_PPV_ARGS(&m_CopyQueue)));

    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Init(   m_Device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, std::numeric_limits<uint16_t>::max(), D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Init(       m_Device.Get(),  D3D12_DESCRIPTOR_HEAP_TYPE_DSV, std::numeric_limits<uint16_t>::max(), D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].Init(   m_Device.Get(),  D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, ESampler::Limit, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Init( m_Device.Get(),  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, std::numeric_limits<uint16_t>::max(), D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

    for (size_t sampler_index = 0; sampler_index < ESampler::Count; sampler_index++)
        m_Device->CreateSampler(&SAMPLER_DESC[sampler_index], m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].GetCPUDescriptorHandle(ResourceID(sampler_index)));

    D3D12_ROOT_PARAMETER1 constants_param = {};
    constants_param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    constants_param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    constants_param.Constants.ShaderRegister = 0;
    constants_param.Constants.RegisterSpace = 0;
    constants_param.Constants.Num32BitValues = sRootSignatureSize / sizeof(DWORD);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC vrsd;
    vrsd.Init_1_1(1, &constants_param, ESampler::Count, STATIC_SAMPLER_DESC.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | 
                                                                                    D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED  | 
                                                                                    D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED);
    ComPtr<ID3DBlob> signature, error;
    auto serialize_vrs_hr = D3DX12SerializeVersionedRootSignature(&vrsd, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);

    if (error)
        OutputDebugStringA((char*)error->GetBufferPointer());

    gThrowIfFailed(serialize_vrs_hr);
    gThrowIfFailed(m_Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_GlobalRootSignature)));
}


BufferID Device::CreateBuffer(const Buffer::Desc& desc, const std::wstring& name) {
    auto alloc_desc     = D3D12MA::ALLOCATION_DESC{};
    alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    auto buffer         = Buffer(desc);
    auto initial_state  = D3D12_RESOURCE_STATE_COMMON;
    auto buffer_desc    = D3D12_RESOURCE_DESC(CD3DX12_RESOURCE_DESC::Buffer(desc.size));

    switch (desc.usage) {
        case Buffer::VERTEX_BUFFER : {
            initial_state = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            buffer_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        } break;
        case Buffer::INDEX_BUFFER: {
            initial_state = D3D12_RESOURCE_STATE_INDEX_BUFFER;
            buffer_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        } break;
        case Buffer::UPLOAD: {
            alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
        } break;
        case Buffer::ACCELERATION_STRUCTURE: {
            initial_state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
            buffer_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        } break;
    }
    
    gThrowIfFailed(m_Allocator->CreateResource(
        &alloc_desc, 
        &buffer_desc,
        initial_state,
        nullptr, 
        buffer.m_Allocation.GetAddressOf(), 
        IID_PPV_ARGS(&buffer.m_Resource)
    ));

    if (desc.stride) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
        uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uav_desc.Buffer.StructureByteStride = desc.stride;
        uav_desc.Buffer.NumElements = desc.size / desc.stride;
        buffer.m_View = CreateUnorderedAccessView(buffer.m_Resource, &uav_desc);
    }
    
    if (!name.empty())
        buffer.m_Resource->SetName(name.c_str());

    return m_Buffers.Add(buffer);
}


TextureID Device::CreateTextureView(TextureID inTextureID, const Texture::Desc& desc) {
    // make a copy of the texture
    const auto& texture = GetTexture(inTextureID);
    Texture new_texture = texture;
    new_texture.m_Description.usage = desc.usage;
    new_texture.m_Description.viewDesc = desc.viewDesc;

    switch (desc.usage) {
    case Texture::DEPTH_STENCIL_TARGET: {
        new_texture.m_View = CreateDepthStencilView(texture.m_Resource, static_cast<D3D12_DEPTH_STENCIL_VIEW_DESC*>(desc.viewDesc));
    } break;
    case Texture::RENDER_TARGET:
        new_texture.m_View = CreateRenderTargetView(texture.m_Resource, static_cast<D3D12_RENDER_TARGET_VIEW_DESC*>(desc.viewDesc));
        break;
    case Texture::SHADER_SAMPLE:
        new_texture.m_View = CreateShaderResourceView(texture.m_Resource, static_cast<D3D12_SHADER_RESOURCE_VIEW_DESC*>(desc.viewDesc));
        break;
    case Texture::SHADER_READ:
    case Texture::SHADER_WRITE:
        new_texture.m_View = CreateUnorderedAccessView(texture.m_Resource, static_cast<D3D12_UNORDERED_ACCESS_VIEW_DESC*>(desc.viewDesc));
        break;
    default:
        assert(false); // should not be able to get here
    }

    return m_Textures.Add(new_texture);
}


BufferID Device::CreateBufferView(BufferID inBufferID, const Buffer::Desc& desc) {
    const auto& buffer = GetBuffer(inBufferID);
    Buffer new_buffer = buffer;
    new_buffer.m_Description.usage = desc.usage;

    if (desc.stride) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
        uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uav_desc.Buffer.StructureByteStride = desc.stride;
        uav_desc.Buffer.NumElements = desc.size / desc.stride;
        new_buffer.m_View = CreateUnorderedAccessView(buffer.m_Resource, &uav_desc);
    }

    return m_Buffers.Add(new_buffer);
}


TextureID Device::CreateTexture(const Texture::Desc& desc, const std::wstring& name) {
    auto texture = Texture(desc);
    auto resource_desc = D3D12_RESOURCE_DESC(CD3DX12_RESOURCE_DESC::Tex2D(
        desc.format, desc.width, desc.height, 1u, desc.mipLevels, 1u, 0, D3D12_RESOURCE_FLAG_NONE, D3D12_TEXTURE_LAYOUT_UNKNOWN)
    );

    auto alloc_desc = D3D12MA::ALLOCATION_DESC{};
    alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    auto initial_state = D3D12_RESOURCE_STATE_COMMON;
    D3D12_CLEAR_VALUE* clear_value = nullptr;

    switch (desc.usage) {
        case Texture::DEPTH_STENCIL_TARGET: {
            resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            initial_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;

            clear_value = new CD3DX12_CLEAR_VALUE(desc.format, 1.0f, 0.0f);
            if (desc.viewDesc)
                clear_value->Format = static_cast<D3D12_DEPTH_STENCIL_VIEW_DESC*>(desc.viewDesc)->Format;
        } break;

        case Texture::RENDER_TARGET: {
            resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;

            float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            clear_value = new CD3DX12_CLEAR_VALUE(desc.format, color);
            if (desc.viewDesc)
                clear_value->Format = static_cast<D3D12_RENDER_TARGET_VIEW_DESC*>(desc.viewDesc)->Format;
        } break;

        case Texture::SHADER_SAMPLE: {
            initial_state = D3D12_RESOURCE_STATE_COPY_DEST;
        } break;

        case Texture::SHADER_WRITE: {
            resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;;
            initial_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        } break;
    }

    gThrowIfFailed(m_Allocator->CreateResource(
        &alloc_desc, 
        &resource_desc, 
        initial_state, 
        clear_value,
        texture.m_Allocation.GetAddressOf(),
        IID_PPV_ARGS(&texture.m_Resource)
    ));

    switch (desc.usage) {
        case Texture::DEPTH_STENCIL_TARGET: {
            texture.m_View = CreateDepthStencilView(texture.m_Resource, static_cast<D3D12_DEPTH_STENCIL_VIEW_DESC*>(desc.viewDesc));
        } break;
        case Texture::RENDER_TARGET:
            texture.m_View = CreateRenderTargetView(texture.m_Resource, static_cast<D3D12_RENDER_TARGET_VIEW_DESC*>(desc.viewDesc));
            break;
        case Texture::SHADER_SAMPLE:
            texture.m_View = CreateShaderResourceView(texture.m_Resource, static_cast<D3D12_SHADER_RESOURCE_VIEW_DESC*>(desc.viewDesc));
            break;
        case Texture::SHADER_READ:
        case Texture::SHADER_WRITE:
            texture.m_View = CreateUnorderedAccessView(texture.m_Resource, static_cast<D3D12_UNORDERED_ACCESS_VIEW_DESC*>(desc.viewDesc));
            break;
        default:
            assert(false); // should not be able to get here
    }
    
    if (!name.empty())
        texture.m_Resource->SetName(name.c_str());

    return m_Textures.Add(texture);
}


ResourceID Device::CreateDepthStencilView(ResourceRef inResource, D3D12_DEPTH_STENCIL_VIEW_DESC* inDesc) {
    auto& heap = m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV];
    const auto descriptor_id = heap.Add(inResource);
    m_Device->CreateDepthStencilView(inResource.Get(), inDesc, heap.GetCPUDescriptorHandle(descriptor_id));
    return descriptor_id;
}


ResourceID Device::CreateRenderTargetView(ResourceRef inResource, D3D12_RENDER_TARGET_VIEW_DESC* inDesc) {
    auto& heap = m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
    const auto descriptor_id = heap.Add(inResource);
    m_Device->CreateRenderTargetView(inResource.Get(), inDesc, heap.GetCPUDescriptorHandle(descriptor_id));
    return descriptor_id;
}


ResourceID Device::CreateShaderResourceView(ResourceRef inResource, D3D12_SHADER_RESOURCE_VIEW_DESC* inDesc) {
    auto& heap = m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    const auto descriptor_id = heap.Add(inResource);
    m_Device->CreateShaderResourceView(inResource.Get(), inDesc, heap.GetCPUDescriptorHandle(descriptor_id));
    return descriptor_id;
}


ResourceID Device::CreateUnorderedAccessView(ResourceRef inResource, D3D12_UNORDERED_ACCESS_VIEW_DESC* inDesc) {
    auto& heap = m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    const auto descriptor_id = heap.Add(inResource);
    m_Device->CreateUnorderedAccessView(inResource.Get(), nullptr, inDesc, heap.GetCPUDescriptorHandle(descriptor_id));
    return descriptor_id;
}


StagingHeap::~StagingHeap() {
    for (auto& buffer_view : m_Buffers)
        m_Device.GetBuffer(buffer_view.mBufferID)->Unmap(0, nullptr);
}

void StagingHeap::StageBuffer(ID3D12GraphicsCommandList* inCmdList, ResourceRef inResource, size_t inOffset, const void* inData, size_t inSize) {
    for (auto& buffer : m_Buffers) {
        if (buffer.mRetired && inSize < buffer.mCapacity - buffer.mSize) {
            memcpy(buffer.mPtr + buffer.mSize, inData, inSize);

            const auto buffer_resource = m_Device.GetBuffer(buffer.mBufferID).GetResource();
            inCmdList->CopyBufferRegion(inResource.Get(), inOffset, buffer_resource.Get(), buffer.mSize, inSize);

            buffer.mSize += inSize;
            buffer.mRetired = false;
            return;
        }
    }

    auto buffer_id = m_Device.CreateBuffer(Buffer::Desc{
        .size = inSize,
        .usage = Buffer::Usage::UPLOAD
        });

    auto& buffer = m_Device.GetBuffer(buffer_id);

    uint8_t* mapped_ptr;
    CD3DX12_RANGE range = CD3DX12_RANGE(0, 0);
    gThrowIfFailed(buffer->Map(0, &range, reinterpret_cast<void**>(&mapped_ptr)));

    memcpy(mapped_ptr, inData, inSize);

    inCmdList->CopyBufferRegion(inResource.Get(), inOffset, buffer.GetResource().Get(), 0, inSize);

    m_Buffers.emplace_back(StagingBuffer{
        .mSize = inSize,
        .mCapacity = inSize,
        .mRetired = false,
        .mPtr = mapped_ptr,
        .mBufferID = buffer_id
    });
}

void StagingHeap::StageTexture(ID3D12GraphicsCommandList* inCmdList, ResourceRef inResource, size_t inSubResource, const void* inData) {
    auto desc = inResource->GetDesc();
    auto font_texture_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8_UNORM, desc.Width, desc.Height);

    UINT rows;
    UINT64 row_size, total_size;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT font_texture_footprint;
    m_Device->GetCopyableFootprints(&font_texture_desc, 0, 1, 0, &font_texture_footprint, &rows, &row_size, &total_size);

    const auto aligned_size = gAlignUp(font_texture_footprint.Footprint.RowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

    for (auto& buffer : m_Buffers) {
        if (buffer.mRetired && total_size < buffer.mCapacity - buffer.mSize) {
            memcpy(buffer.mPtr + buffer.mSize, inData, aligned_size);

            const auto buffer_resource = m_Device.GetBuffer(buffer.mBufferID).GetResource();
            const auto dest = CD3DX12_TEXTURE_COPY_LOCATION(inResource.Get(), inSubResource);
            const auto source = CD3DX12_TEXTURE_COPY_LOCATION(buffer_resource.Get(), font_texture_footprint);

            inCmdList->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);

            buffer.mSize += aligned_size;
            buffer.mRetired = false;

            return;
        }
    }

    auto buffer_id = m_Device.CreateBuffer(Buffer::Desc{
        .size = total_size,
        .usage = Buffer::Usage::UPLOAD
        });

    auto& buffer = m_Device.GetBuffer(buffer_id);

    uint8_t* mapped_ptr;
    CD3DX12_RANGE range = CD3DX12_RANGE(0, 0);
    gThrowIfFailed(buffer->Map(0, &range, reinterpret_cast<void**>(&mapped_ptr)));

    memcpy(mapped_ptr, inData, aligned_size);

    const auto dest = CD3DX12_TEXTURE_COPY_LOCATION(inResource.Get(), inSubResource);
    const auto source = CD3DX12_TEXTURE_COPY_LOCATION(buffer.GetResource().Get(), font_texture_footprint);

    inCmdList->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);

    m_Buffers.emplace_back(StagingBuffer{
        .mSize = aligned_size,
        .mCapacity = total_size,
        .mRetired = false,
        .mPtr = mapped_ptr,
        .mBufferID = buffer_id
    });
}


}
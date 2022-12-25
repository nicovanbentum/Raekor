#include "pch.h"
#include "DXDevice.h"
#include "DXSampler.h"
#include "DXUtil.h"
#include "DXCommandList.h"
#include "DXRenderGraph.h"
#include "Raekor/timer.h"

namespace Raekor::DX {

Device::Device(SDL_Window* window, uint32_t inFrameCount) : m_NumFrames(inFrameCount) {
    UINT device_creation_flags = 0;

#ifndef NDEBUG
    ComPtr<ID3D12Debug1> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
        debug->EnableDebugLayer();
        debug->SetEnableGPUBasedValidation(TRUE);
        debug->SetEnableSynchronizedCommandQueueValidation(TRUE);
    }

    device_creation_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    ComPtr<IDXGIFactory6> factory;
    gThrowIfFailed(CreateDXGIFactory2(device_creation_flags, IID_PPV_ARGS(&factory)));

    factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&m_Adapter));

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

    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Init(m_Device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, std::numeric_limits<uint16_t>::max(), D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Init(m_Device.Get(),  D3D12_DESCRIPTOR_HEAP_TYPE_DSV, std::numeric_limits<uint16_t>::max(), D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].Init(m_Device.Get(),  D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, ESamplerIndex::SAMPLER_LIMIT, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Init(m_Device.Get(),  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, std::numeric_limits<uint16_t>::max(), D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

    for (size_t sampler_index = 0; sampler_index < ESamplerIndex::SAMPLER_COUNT; sampler_index++)
        m_Device->CreateSampler(&SAMPLER_DESC[sampler_index], m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].GetCPUDescriptorHandle(DescriptorID(sampler_index)));

    std::vector<D3D12_ROOT_PARAMETER1> root_params;

    D3D12_ROOT_PARAMETER1 constants_parameter = {};
    constants_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    constants_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    constants_parameter.Constants.RegisterSpace = 0;
    constants_parameter.Constants.ShaderRegister = 0;
    constants_parameter.Constants.Num32BitValues = sMaxRootConstantsSize / sizeof(DWORD);
    root_params.push_back(constants_parameter);

    // start at 1 because the index is mapped to the shader registers, where 0 is occupied by constants
    for (size_t param_index = 1; param_index < EBindSlot::Count; param_index++) {
        D3D12_ROOT_PARAMETER1 param = {};
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        param.Descriptor.RegisterSpace = 0;
        param.Descriptor.ShaderRegister = param_index;

        switch (param_index) {
            case EBindSlot::CBV0: case EBindSlot::CBV1: case EBindSlot::CBV2: case EBindSlot::CBV3:
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; break;

            case EBindSlot::SRV0: case EBindSlot::SRV1: case EBindSlot::SRV2: case EBindSlot::SRV3:
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; break;

            case EBindSlot::UAV0: case EBindSlot::UAV1: case EBindSlot::UAV2: case EBindSlot::UAV3:
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV; break;

            default: assert(false);
        }

        root_params.push_back(param);
    }

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC vrsd;
    vrsd.Init_1_1(root_params.size(), root_params.data(), ESamplerIndex::SAMPLER_COUNT, STATIC_SAMPLER_DESC.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                                                                                    D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED  | 
                                                                                    D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED);
    ComPtr<ID3DBlob> signature, error;
    auto serialize_vrs_hr = D3DX12SerializeVersionedRootSignature(&vrsd, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);

    if (error)
        OutputDebugStringA((char*)error->GetBufferPointer());

    gThrowIfFailed(serialize_vrs_hr);
    gThrowIfFailed(m_Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_GlobalRootSignature)));
}



void Device::BindDrawDefaults(CommandList& inCmdList) {
    inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const auto heaps = std::array {
        GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).GetHeap(),
        GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetHeap()
    };

    inCmdList->SetDescriptorHeaps(heaps.size(), heaps.data());
    inCmdList->SetComputeRootSignature(GetGlobalRootSignature());
    inCmdList->SetGraphicsRootSignature(GetGlobalRootSignature());
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



TextureID Device::CreateTextureView(TextureID inTextureID, const Texture::Desc& inDesc) {
    // make a copy of the texture
    const auto& texture = GetTexture(inTextureID);
    Texture new_texture = texture;
    new_texture.m_Desc.usage = inDesc.usage;
    new_texture.m_Desc.viewDesc = inDesc.viewDesc;

    const auto texture_id = m_Textures.Add(new_texture);
    CreateDescriptor(texture_id, inDesc);

    return texture_id;
}



[[nodiscard]] TextureID Device::CreateTextureView(ResourceRef inResource, const Texture::Desc& inDesc) {
    auto temp_texture = Texture(inDesc);
    temp_texture.m_Resource = inResource;
    const auto texture_id = m_Textures.Add(temp_texture);
    
    auto& texture = GetTexture(texture_id);
    texture.m_Desc.format = texture.GetResource()->GetDesc().Format;
    texture.m_Desc.usage = inDesc.usage;
    texture.m_Desc.viewDesc = inDesc.viewDesc;
    CreateDescriptor(texture_id, inDesc);

    return texture_id;
}



void Device::ReleaseBuffer(BufferID inBufferID) { 
    assert(inBufferID.IsValid());
    m_Buffers.Remove(inBufferID);
    // ReleaseDescriptor(inBufferID);
}


void Device::ReleaseTexture(TextureID inTextureID) { 
    assert(inTextureID.IsValid());
    m_Textures.Remove(inTextureID);
    ReleaseDescriptor(inTextureID);
}


void Device::ReleaseBufferImmediate(BufferID inBufferID) {
    assert(inBufferID.IsValid());

    auto& buffer = GetBuffer(inBufferID);
    buffer.m_Resource = nullptr;

    m_Buffers.Remove(inBufferID);
    // ReleaseDescriptorImmediate(inBufferID);
}


void Device::ReleaseTextureImmediate(TextureID inTextureID) {
    assert(inTextureID.IsValid());

    auto& texture = GetTexture(inTextureID);
    texture.m_Resource = nullptr;

    m_Textures.Remove(inTextureID);
    ReleaseDescriptorImmediate(inTextureID);
}



D3D12_GRAPHICS_PIPELINE_STATE_DESC Device::CreatePipelineStateDesc(IRenderPass* inRenderPass, const std::string& inVertexShader, const std::string& inPixelShader)
{
    const auto& pixelShader  = m_Shaders.at(inPixelShader);
    const auto& vertexShader = m_Shaders.at(inVertexShader);

    static constexpr std::array vertex_layout = {
        D3D12_INPUT_ELEMENT_DESC { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
    state.VS                                      = CD3DX12_SHADER_BYTECODE(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
    state.PS                                      = CD3DX12_SHADER_BYTECODE(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
    state.PrimitiveTopologyType                   = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    state.InputLayout.NumElements                 = vertex_layout.size();
    state.InputLayout.pInputElementDescs          = vertex_layout.data();
    state.BlendState                              = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    state.RasterizerState                         = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    state.RasterizerState.FrontCounterClockwise   = TRUE;
    state.DepthStencilState                       = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    state.SampleMask                              = UINT_MAX;
    state.SampleDesc.Count                        = 1;
    state.pRootSignature                          = GetGlobalRootSignature();

    for (const auto& resource : inRenderPass->m_WrittenTextures) {
        const auto& texture = GetTexture(resource.mResourceTexture);

        switch (texture.GetDesc().usage) {
            case Texture::RENDER_TARGET:
                state.RTVFormats[state.NumRenderTargets++] = texture.GetDesc().format;
                break;
            case Texture::DEPTH_STENCIL_TARGET:
                assert(state.DSVFormat == DXGI_FORMAT_UNKNOWN); // If you define multiple depth targets, I got bad news for you
                state.DSVFormat = texture.GetDesc().format;
                break;
        }
    }

    return state;
}

D3D12_COMPUTE_PIPELINE_STATE_DESC Device::CreatePipelineStateDesc(IRenderPass* inRenderPass, const std::string& inComputeShader) {
    const auto& compute_shader = m_Shaders.at(inComputeShader);

    D3D12_COMPUTE_PIPELINE_STATE_DESC state = {};
    state.CS = CD3DX12_SHADER_BYTECODE(compute_shader->GetBufferPointer(), compute_shader->GetBufferSize());
    state.pRootSignature = GetGlobalRootSignature();

    return state;
 }



void Device::CreateDescriptor(TextureID inID, const Texture::Desc& inDesc) {
    auto& texture = GetTexture(inID);
    texture.m_Desc.usage = inDesc.usage;
    texture.m_Desc.viewDesc = inDesc.viewDesc;

    switch (inDesc.usage) {
    case Texture::DEPTH_STENCIL_TARGET: {
        texture.m_View = CreateDepthStencilView(texture.m_Resource, static_cast<D3D12_DEPTH_STENCIL_VIEW_DESC*>(inDesc.viewDesc));
    } break;
    case Texture::RENDER_TARGET:
        texture.m_View = CreateRenderTargetView(texture.m_Resource, static_cast<D3D12_RENDER_TARGET_VIEW_DESC*>(inDesc.viewDesc));
        break;
    case Texture::SHADER_READ_ONLY:
        texture.m_View = CreateShaderResourceView(texture.m_Resource, static_cast<D3D12_SHADER_RESOURCE_VIEW_DESC*>(inDesc.viewDesc));
        break;
    case Texture::SHADER_READ_WRITE:
        texture.m_View = CreateUnorderedAccessView(texture.m_Resource, static_cast<D3D12_UNORDERED_ACCESS_VIEW_DESC*>(inDesc.viewDesc));
        break;
    default:
        assert(false); // should not be able to get here
    }
}



void Device::ReleaseDescriptor(TextureID inTextureID) {
    auto& texture = GetTexture(inTextureID);
    m_Heaps[gGetHeapType(texture.m_Desc.usage)].Remove(texture.m_View);
}



void Device::ReleaseDescriptorImmediate(TextureID inTextureID) {
    auto& texture = GetTexture(inTextureID);
    m_Heaps[gGetHeapType(texture.m_Desc.usage)].Get(texture.m_View) = nullptr;
    ReleaseDescriptor(inTextureID);
}



BufferID Device::CreateBufferView(BufferID inBufferID, const Buffer::Desc& desc) {
    const auto& buffer = GetBuffer(inBufferID);
    Buffer new_buffer = buffer;
    new_buffer.m_Desc.usage = desc.usage;

    if (desc.stride) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
        uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uav_desc.Buffer.StructureByteStride = desc.stride;
        uav_desc.Buffer.NumElements = desc.size / desc.stride;
        new_buffer.m_View = CreateUnorderedAccessView(buffer.m_Resource, &uav_desc);
    }

    return m_Buffers.Add(new_buffer);
}



TextureID Device::CreateTexture(const Texture::Desc& desc, const std::wstring& inName) {
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
            initial_state       = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            alloc_desc.Flags    = D3D12MA::ALLOCATION_FLAG_COMMITTED;
            resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            clear_value = new CD3DX12_CLEAR_VALUE(desc.format, 1.0f, 0.0f);
            if (desc.viewDesc)
                clear_value->Format = static_cast<D3D12_DEPTH_STENCIL_VIEW_DESC*>(desc.viewDesc)->Format;
        } break;

        case Texture::RENDER_TARGET: {
            initial_state       = D3D12_RESOURCE_STATE_RENDER_TARGET;
            alloc_desc.Flags    = D3D12MA::ALLOCATION_FLAG_COMMITTED;
            resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

            float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            clear_value = new CD3DX12_CLEAR_VALUE(desc.format, color);
            if (desc.viewDesc)
                clear_value->Format = static_cast<D3D12_RENDER_TARGET_VIEW_DESC*>(desc.viewDesc)->Format;
        } break;

        case Texture::SHADER_READ_ONLY: {
            initial_state = D3D12_RESOURCE_STATE_COPY_DEST;
        } break;

        case Texture::SHADER_READ_WRITE: {
            initial_state       = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
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

    if (!inName.empty())
        texture.m_Resource->SetName(inName.c_str());

    const auto texture_id = m_Textures.Add(texture);
    CreateDescriptor(texture_id, desc);

    return texture_id;
}



D3D12_CPU_DESCRIPTOR_HANDLE Device::GetCPUDescriptorHandle(TextureID inID) {
    auto& texture = GetTexture(inID);

    switch (texture.GetDesc().usage) {
        case Texture::RENDER_TARGET:
            return GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV).GetCPUDescriptorHandle(texture.GetView());
        case Texture::DEPTH_STENCIL_TARGET:
            return GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV).GetCPUDescriptorHandle(texture.GetView());
        case Texture::SHADER_READ_ONLY:
        case Texture::SHADER_READ_WRITE:
            return GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetCPUDescriptorHandle(texture.GetView());
        default:
            assert(false);
    }

    assert(false);
    return CD3DX12_CPU_DESCRIPTOR_HANDLE();
}

D3D12_CPU_DESCRIPTOR_HANDLE Device::GetHeapPtr(TextureResource inResource)
{
    return GetCPUDescriptorHandle(inResource.mResourceTexture);
}



DescriptorID Device::CreateDepthStencilView(ResourceRef inResource, D3D12_DEPTH_STENCIL_VIEW_DESC* inDesc) {
    auto& heap = m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV];
    const auto descriptor_id = heap.Add(inResource);
    m_Device->CreateDepthStencilView(inResource.Get(), inDesc, heap.GetCPUDescriptorHandle(descriptor_id));
    return descriptor_id;
}



DescriptorID Device::CreateRenderTargetView(ResourceRef inResource, D3D12_RENDER_TARGET_VIEW_DESC* inDesc) {
    auto& heap = m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
    const auto descriptor_id = heap.Add(inResource);
    Timer timer;
    m_Device->CreateRenderTargetView(inResource.Get(), inDesc, heap.GetCPUDescriptorHandle(descriptor_id));
    return descriptor_id;
}



DescriptorID Device::CreateShaderResourceView(ResourceRef inResource, D3D12_SHADER_RESOURCE_VIEW_DESC* inDesc) {
    auto& heap = m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    const auto descriptor_id = heap.Add(inResource);
    m_Device->CreateShaderResourceView(inResource.Get(), inDesc, heap.GetCPUDescriptorHandle(descriptor_id));
    return descriptor_id;
}



DescriptorID Device::CreateUnorderedAccessView(ResourceRef inResource, D3D12_UNORDERED_ACCESS_VIEW_DESC* inDesc) {
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
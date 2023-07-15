#include "pch.h"
#include "DXDevice.h"
#include "DXSampler.h"
#include "DXUtil.h"
#include "DXCommandList.h"
#include "DXRenderGraph.h"
#include "Raekor/timer.h"

#include <locale>
#include <codecvt>

namespace Raekor::DX12 {

Device::Device(SDL_Window* window, uint32_t inFrameCount) : m_NumFrames(inFrameCount) {
    auto device_creation_flags = 0u;

#ifndef NDEBUG
    auto debug_interface = ComPtr<ID3D12Debug1>{};

    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface)))) {
        if (CVars::sCreate("debug_layer", 0)) 
            debug_interface->EnableDebugLayer();


        if (CVars::sCreate("gpu_validation", 0)) {
            debug_interface->SetEnableGPUBasedValidation(TRUE);
            debug_interface->SetEnableSynchronizedCommandQueueValidation(TRUE);
        }
    }

    device_creation_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    
    if (CVars::sCreate("hook_pix", 0)) {
        auto pix_module = PIXLoadLatestWinPixGpuCapturerLibrary();
        assert(pix_module);
    }

    m_Buffers.Reserve(UINT16_MAX);
    m_Textures.Reserve(UINT16_MAX);

    auto factory = ComPtr<IDXGIFactory6>{};
    gThrowIfFailed(CreateDXGIFactory2(device_creation_flags, IID_PPV_ARGS(&factory)));
    factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&m_Adapter));
    gThrowIfFailed(D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&m_Device)));

    const auto allocator_desc = D3D12MA::ALLOCATOR_DESC { .pDevice = m_Device.Get(), .pAdapter = m_Adapter.Get() };
    gThrowIfFailed(D3D12MA::CreateAllocator(&allocator_desc, &m_Allocator));

    constexpr auto copy_queue_desc   = D3D12_COMMAND_QUEUE_DESC { .Type = D3D12_COMMAND_LIST_TYPE_COPY };
    constexpr auto direct_queue_desc = D3D12_COMMAND_QUEUE_DESC { .Type = D3D12_COMMAND_LIST_TYPE_DIRECT };
    gThrowIfFailed(m_Device->CreateCommandQueue(&copy_queue_desc, IID_PPV_ARGS(&m_CopyQueue)));
    gThrowIfFailed(m_Device->CreateCommandQueue(&direct_queue_desc, IID_PPV_ARGS(&m_Queue)));

    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Init(m_Device.Get(),  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, std::numeric_limits<uint16_t>::max(), D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].Init(m_Device.Get(),  D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, ESamplerIndex::SAMPLER_LIMIT, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Init(m_Device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, std::numeric_limits<uint8_t>::max(), D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Init(m_Device.Get(),  D3D12_DESCRIPTOR_HEAP_TYPE_DSV, std::numeric_limits<uint8_t>::max(), D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

    for (size_t sampler_index = 0; sampler_index < ESamplerIndex::SAMPLER_COUNT; sampler_index++)
        m_Device->CreateSampler(&SAMPLER_DESC[sampler_index], m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].GetCPUDescriptorHandle(DescriptorID(sampler_index)));

    auto root_params = std::vector<D3D12_ROOT_PARAMETER1>{};
    auto b_registers = 0u, t_registers = 0u;

    root_params.emplace_back(D3D12_ROOT_PARAMETER1 {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
        .Constants = D3D12_ROOT_CONSTANTS {
            .ShaderRegister = b_registers++,
            .RegisterSpace = 0u,
            .Num32BitValues = sMaxRootConstantsSize / (uint32_t)sizeof(DWORD),
        },
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
    });

    for (uint32_t bind_slot = EBindSlot::SRV0; bind_slot < EBindSlot::Count; bind_slot++) {
        auto param = D3D12_ROOT_PARAMETER1 {
            .Descriptor = D3D12_ROOT_DESCRIPTOR1 { .RegisterSpace = 0 },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        };

        switch (bind_slot) {
            case EBindSlot::SRV0: case EBindSlot::SRV1:
                param.Descriptor.ShaderRegister = t_registers++;
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; 
                break;
            default: assert(false);
        }

        root_params.push_back(param);
    }


    auto vrsd = CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC{};
    vrsd.Init_1_1(
        root_params.size(), root_params.data(), 
        ESamplerIndex::SAMPLER_COUNT, STATIC_SAMPLER_DESC.data(), 
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED  | 
        D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
    );

    auto signature = ComPtr<ID3DBlob>{}, error = ComPtr<ID3DBlob>{};
    auto serialize_vrs_hr = D3DX12SerializeVersionedRootSignature(&vrsd, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);

    if (error)
        OutputDebugStringA((char*)error->GetBufferPointer());

    gThrowIfFailed(serialize_vrs_hr);
    gThrowIfFailed(m_Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_GlobalRootSignature)));

    gThrowIfFailed(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &mIsTearingSupported, sizeof(mIsTearingSupported)));
}



void Device::BindDrawDefaults(CommandList& inCmdList) {
    inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const auto heaps = std::array {
        GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetHeap(),
        GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).GetHeap()
    };

    inCmdList->SetDescriptorHeaps(heaps.size(), heaps.data());
    inCmdList->SetComputeRootSignature(GetGlobalRootSignature());
    inCmdList->SetGraphicsRootSignature(GetGlobalRootSignature());
}



TextureID Device::CreateTexture(const Texture::Desc& inDesc, const std::wstring& inName) {
    auto texture = Texture(inDesc);
    auto resource_desc = D3D12_RESOURCE_DESC(CD3DX12_RESOURCE_DESC::Tex2D(
        inDesc.format, inDesc.width, inDesc.height, inDesc.arrayLayers, inDesc.mipLevels, 1u, 0, D3D12_RESOURCE_FLAG_NONE, D3D12_TEXTURE_LAYOUT_UNKNOWN)
    );

    auto alloc_desc = D3D12MA::ALLOCATION_DESC{};
    alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    auto clear_value = D3D12_CLEAR_VALUE{};
    auto clear_value_ptr = static_cast<D3D12_CLEAR_VALUE*>(nullptr);

    auto initial_state = D3D12_RESOURCE_STATE_COMMON;

    switch (inDesc.usage) {
    case Texture::DEPTH_STENCIL_TARGET: {
        initial_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        alloc_desc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
        resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        clear_value = CD3DX12_CLEAR_VALUE(inDesc.format, 1.0f, 0.0f);
        clear_value_ptr = &clear_value;

        if (inDesc.viewDesc)
            clear_value.Format = static_cast<D3D12_DEPTH_STENCIL_VIEW_DESC*>(inDesc.viewDesc)->Format;
    } break;

    case Texture::RENDER_TARGET: {
        initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
        alloc_desc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
        resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        clear_value = CD3DX12_CLEAR_VALUE(inDesc.format, color);
        clear_value_ptr = &clear_value;

        if (inDesc.viewDesc)
            clear_value.Format = static_cast<D3D12_RENDER_TARGET_VIEW_DESC*>(inDesc.viewDesc)->Format;
    } break;

    case Texture::SHADER_READ_ONLY: {
        initial_state = D3D12_RESOURCE_STATE_COPY_DEST;
    } break;

    case Texture::SHADER_READ_WRITE: {
        initial_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    } break;
    }

    gThrowIfFailed(m_Allocator->CreateResource(
        &alloc_desc,
        &resource_desc,
        initial_state,
        clear_value_ptr,
        texture.m_Allocation.GetAddressOf(),
        IID_PPV_ARGS(&texture.m_Resource)
    ));

    if (!inName.empty())
        texture.m_Resource->SetName(inName.c_str());

    const auto texture_id = m_Textures.Add(texture);
    CreateDescriptor(texture_id, inDesc);

    return texture_id;
}



BufferID Device::CreateBuffer(const Buffer::Desc& inDesc, const std::wstring& inName) {
    auto alloc_desc     = D3D12MA::ALLOCATION_DESC{};
    alloc_desc.HeapType = inDesc.mappable ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;

    auto buffer         = Buffer(inDesc);
    auto initial_state  = D3D12_RESOURCE_STATE_COMMON;
    auto buffer_desc    = D3D12_RESOURCE_DESC(CD3DX12_RESOURCE_DESC::Buffer(inDesc.size));

    switch (inDesc.usage) {
        case Buffer::VERTEX_BUFFER:
            initial_state = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            if (!inDesc.mappable)
                buffer_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            break;
        case Buffer::INDEX_BUFFER:
            initial_state = D3D12_RESOURCE_STATE_INDEX_BUFFER;
            if (!inDesc.mappable)
                buffer_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            break;
        case Buffer::UPLOAD:
        case Buffer::SHADER_READ_ONLY:
            alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
            break;
        case Buffer::SHADER_READ_WRITE:
            initial_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            buffer_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            break;
        case Buffer::ACCELERATION_STRUCTURE:
            initial_state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
            buffer_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            break;
    }

    gThrowIfFailed(m_Allocator->CreateResource(
        &alloc_desc, 
        &buffer_desc,
        initial_state,
        nullptr, 
        buffer.m_Allocation.GetAddressOf(), 
        IID_PPV_ARGS(&buffer.m_Resource)
    ));
    
    if (!inName.empty())
        buffer.m_Resource->SetName(inName.c_str());

    const auto buffer_id = m_Buffers.Add(buffer);
    CreateDescriptor(buffer_id, inDesc);

    return buffer_id;
}



BufferID Device::CreateBufferView(BufferID inBufferID, const Buffer::Desc& inDesc) {
    // make a copy of the texture
    const auto& buffer = GetBuffer(inBufferID);
    auto new_buffer = Buffer(buffer);
    new_buffer.m_Desc.usage = inDesc.usage;
    new_buffer.m_Desc.viewDesc = inDesc.viewDesc;

    const auto buffer_id = m_Buffers.Add(new_buffer);
    CreateDescriptor(buffer_id, inDesc);

    return buffer_id;
}



TextureID Device::CreateTextureView(TextureID inTextureID, const Texture::Desc& inDesc) {
    // make a copy of the texture
    const auto& texture = GetTexture(inTextureID);
    auto new_texture = Texture(texture);
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



D3D12_GRAPHICS_PIPELINE_STATE_DESC Device::CreatePipelineStateDesc(IRenderPass* inRenderPass, const std::string& inVertexShader, const std::string& inPixelShader) {
    assert(m_Shaders.contains(inPixelShader));
    assert(m_Shaders.contains(inVertexShader));
    const auto& pixelShader  = m_Shaders.at(inPixelShader);
    const auto& vertexShader = m_Shaders.at(inVertexShader);

    return CreatePipelineStateDesc(inRenderPass,
        CD3DX12_SHADER_BYTECODE(vertexShader.mBlob->GetBufferPointer(), vertexShader.mBlob->GetBufferSize()),
        CD3DX12_SHADER_BYTECODE(pixelShader.mBlob->GetBufferPointer(),  pixelShader.mBlob->GetBufferSize()));
}



D3D12_GRAPHICS_PIPELINE_STATE_DESC Device::CreatePipelineStateDesc(IRenderPass* inRenderPass, const CD3DX12_SHADER_BYTECODE& inVertexShader, const CD3DX12_SHADER_BYTECODE& inPixelShader) {
    assert(inRenderPass->IsGraphics() && "Cannot create a Graphics PSO description for a Compute RenderPass");
    
    static constexpr auto vertex_layout = std::array {
        D3D12_INPUT_ELEMENT_DESC { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    auto pso_state = D3D12_GRAPHICS_PIPELINE_STATE_DESC {
        .pRootSignature = GetGlobalRootSignature(),
        .VS = inVertexShader,
        .PS = inPixelShader,
        .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
        .SampleMask = UINT_MAX,
        .RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
        .DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
        .InputLayout = D3D12_INPUT_LAYOUT_DESC {
            .pInputElementDescs = vertex_layout.data(),
            .NumElements = vertex_layout.size(),
        },
        .PrimitiveTopologyType  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .SampleDesc             = DXGI_SAMPLE_DESC {.Count = 1 },
    };
    
    pso_state.RasterizerState.FrontCounterClockwise = TRUE;

    for (const auto& resource : inRenderPass->m_WrittenTextures) {
        const auto& texture = GetTexture(resource.mResourceTexture);

        switch (texture.GetDesc().usage) {
        case Texture::RENDER_TARGET:
            pso_state.RTVFormats[pso_state.NumRenderTargets++] = texture.GetDesc().format;
            break;
        case Texture::DEPTH_STENCIL_TARGET:
            assert(pso_state.DSVFormat == DXGI_FORMAT_UNKNOWN); // If you define multiple depth targets, I got bad news for you
            pso_state.DSVFormat = texture.GetDesc().format;
            break;
        }
    }

    return pso_state;
}



D3D12_COMPUTE_PIPELINE_STATE_DESC Device::CreatePipelineStateDesc(IRenderPass* inRenderPass, const std::string& inComputeShader) {
    const auto& compute_shader = m_Shaders.at(inComputeShader);
    return CreatePipelineStateDesc(inRenderPass, CD3DX12_SHADER_BYTECODE(compute_shader.mBlob->GetBufferPointer(), compute_shader.mBlob->GetBufferSize()));
 }



D3D12_COMPUTE_PIPELINE_STATE_DESC Device::CreatePipelineStateDesc(IRenderPass* inRenderPass, const CD3DX12_SHADER_BYTECODE& inComputeShader) {
    assert(inRenderPass->IsCompute() && "Cannot create a Compute PSO description for a Graphics renderpass");
    return D3D12_COMPUTE_PIPELINE_STATE_DESC {
        .pRootSignature = GetGlobalRootSignature(),
        .CS = inComputeShader
    };
}



void Device::QueueShader(const Path& inPath) {
    Async::sQueueJob([this, inPath]() {
        auto compiled_blob = sCompileShaderDXC(inPath);

        std::scoped_lock(m_ShadersLock);

        m_Shaders[inPath.stem().string()] = ShaderEntry{
            .mPath = inPath,
            .mBlob = compiled_blob,
            .mLastWriteTime = FileSystem::last_write_time(inPath),
        };
    });
}



void Device::CreateDescriptor(BufferID inID, const Buffer::Desc& inDesc) {
    auto& buffer = GetBuffer(inID);
    buffer.m_Desc.usage = inDesc.usage;
    buffer.m_Desc.viewDesc = inDesc.viewDesc;

    switch (inDesc.usage) {
    case Buffer::SHADER_READ_ONLY:
        buffer.m_View = CreateShaderResourceView(buffer.m_Resource, static_cast<D3D12_SHADER_RESOURCE_VIEW_DESC*>(inDesc.viewDesc));
        break;
    case Buffer::SHADER_READ_WRITE:
        if (inDesc.viewDesc == nullptr) {
            auto desc = D3D12_UNORDERED_ACCESS_VIEW_DESC{};
            desc.Format = inDesc.format;
            desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

            if (inDesc.stride > 0) {
                desc.Buffer.StructureByteStride = inDesc.stride;
                desc.Buffer.NumElements = inDesc.size / inDesc.stride;
            }

            buffer.m_View = CreateUnorderedAccessView(buffer.m_Resource, &desc);
        } 
        else 
            buffer.m_View = CreateUnorderedAccessView(buffer.m_Resource, static_cast<D3D12_UNORDERED_ACCESS_VIEW_DESC*>(inDesc.viewDesc));
        break;
    case Buffer::ACCELERATION_STRUCTURE:
        if (inDesc.viewDesc)
            buffer.m_View = CreateShaderResourceView(nullptr, static_cast<D3D12_SHADER_RESOURCE_VIEW_DESC*>(inDesc.viewDesc));
        break;
    case Buffer::UPLOAD:
    case Buffer::GENERAL:
    case Buffer::INDEX_BUFFER:
    case Buffer::VERTEX_BUFFER:
        if (inDesc.viewDesc)
            buffer.m_View = CreateShaderResourceView(buffer.m_Resource, static_cast<D3D12_SHADER_RESOURCE_VIEW_DESC*>(inDesc.viewDesc));
        else if (inDesc.stride) {
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srv_desc.Buffer.StructureByteStride = inDesc.stride;
            srv_desc.Buffer.NumElements = inDesc.size / inDesc.stride;

            buffer.m_View = CreateShaderResourceView(buffer.m_Resource, &srv_desc);

        }
        break;
    default:
        assert(false); // should not be able to get here
    }
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
    return CD3DX12_CPU_DESCRIPTOR_HANDLE{};
}



D3D12_CPU_DESCRIPTOR_HANDLE Device::GetCPUDescriptorHandle(BufferID inID) {
    auto& buffer = GetBuffer(inID);
    return GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetCPUDescriptorHandle(buffer.GetView());
}



D3D12_GPU_DESCRIPTOR_HANDLE Device::GetGPUDescriptorHandle(BufferID inID) {
    auto& buffer = GetBuffer(inID);
    return GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetGPUDescriptorHandle(buffer.GetView());
}



D3D12_GPU_DESCRIPTOR_HANDLE Device::GetGPUDescriptorHandle(TextureID inID) {
    auto& texture = GetTexture(inID);
    return GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetGPUDescriptorHandle(texture.GetView());
}



D3D12_CPU_DESCRIPTOR_HANDLE Device::GetHeapPtr(TextureResource inResource)
{
    return GetCPUDescriptorHandle(inResource.mResourceTexture);
}



DescriptorID Device::CreateDepthStencilView(ResourceRef inResource, const D3D12_DEPTH_STENCIL_VIEW_DESC* inDesc) {
    auto& heap = m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV];
    const auto descriptor_id = heap.Add(inResource);
    m_Device->CreateDepthStencilView(inResource.Get(), inDesc, heap.GetCPUDescriptorHandle(descriptor_id));
    return descriptor_id;
}



DescriptorID Device::CreateRenderTargetView(ResourceRef inResource, const D3D12_RENDER_TARGET_VIEW_DESC* inDesc) {
    auto& heap = m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
    const auto descriptor_id = heap.Add(inResource);
    Timer timer;
    m_Device->CreateRenderTargetView(inResource.Get(), inDesc, heap.GetCPUDescriptorHandle(descriptor_id));
    return descriptor_id;
}



DescriptorID Device::CreateShaderResourceView(ResourceRef inResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* inDesc) {
    auto& heap = m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    const auto descriptor_id = heap.Add(inResource);
    m_Device->CreateShaderResourceView(inResource.Get(), inDesc, heap.GetCPUDescriptorHandle(descriptor_id));
    return descriptor_id;
}



DescriptorID Device::CreateUnorderedAccessView(ResourceRef inResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* inDesc) {
    auto& heap = m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    const auto descriptor_id = heap.Add(inResource);
    m_Device->CreateUnorderedAccessView(inResource.Get(), nullptr, inDesc, heap.GetCPUDescriptorHandle(descriptor_id));
    return descriptor_id;
}



StagingHeap::~StagingHeap() {
    for (auto& buffer_view : m_Buffers)
        m_Device.GetBuffer(buffer_view.mBufferID)->Unmap(0, nullptr);
}



void StagingHeap::StageBuffer(ID3D12GraphicsCommandList* inCmdList, ResourceRef inResource, uint32_t inOffset, const void* inData, uint32_t inSize) {
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

    auto buffer_id = m_Device.CreateBuffer(Buffer::Desc {
        .size  = inSize,
        .usage = Buffer::Usage::UPLOAD
    });

    auto& buffer = m_Device.GetBuffer(buffer_id);

    auto mapped_ptr = static_cast<uint8_t*>(nullptr);
    const auto range = CD3DX12_RANGE(0, 0);
    gThrowIfFailed(buffer->Map(0, &range, reinterpret_cast<void**>(&mapped_ptr)));

    memcpy(mapped_ptr, inData, inSize);

    inCmdList->CopyBufferRegion(inResource.Get(), inOffset, buffer.GetResource().Get(), 0, inSize);

    m_Buffers.emplace_back( StagingBuffer {
        .mSize      = inSize,
        .mCapacity  = inSize,
        .mRetired   = false,
        .mPtr       = mapped_ptr,
        .mBufferID  = buffer_id
    });
}



void StagingHeap::StageTexture(ID3D12GraphicsCommandList* inCmdList, ResourceRef inResource, uint32_t inSubResource, const void* inData) {
    auto nr_of_rows = 0u;
    auto row_size = 0ull, total_size = 0ull;
    auto footprint = D3D12_PLACED_SUBRESOURCE_FOOTPRINT {};
    
    auto desc = inResource->GetDesc();
    m_Device->GetCopyableFootprints(&desc, inSubResource, 1, 0, &footprint, &nr_of_rows, &row_size, &total_size);

    const auto aligned_row_size = gAlignUp(row_size, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

    /*for (auto& buffer : m_Buffers) {
        if (buffer.mRetired && total_size < buffer.mCapacity - buffer.mSize) {
            memcpy(buffer.mPtr + buffer.mSize, inData, aligned_size);

            const auto buffer_resource = m_Device.GetBuffer(buffer.mBufferID).GetResource();
            const auto dest   = CD3DX12_TEXTURE_COPY_LOCATION(inResource.Get(), inSubResource);
            const auto source = CD3DX12_TEXTURE_COPY_LOCATION(buffer_resource.Get(), font_texture_footprint);

            inCmdList->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);

            buffer.mSize += aligned_size;
            buffer.mRetired = false;

            return;
        }
    }*/

    auto buffer_id = m_Device.CreateBuffer( Buffer::Desc {
        .size  = uint32_t(total_size),
        .usage = Buffer::Usage::UPLOAD
    });

    auto& buffer = m_Device.GetBuffer(buffer_id);

    auto mapped_ptr = static_cast<uint8_t*>(nullptr);
    const auto range = CD3DX12_RANGE(0, 0);
    gThrowIfFailed(buffer->Map(0, &range, reinterpret_cast<void**>(&mapped_ptr)));

    mapped_ptr += footprint.Offset;
    auto data_ptr = static_cast<const uint8_t*>(inData);

    for (uint32_t row = 0u; row < nr_of_rows; row++) {
        const auto copy_src = data_ptr + row * row_size;
        const auto copy_dst = mapped_ptr + row * aligned_row_size;
        memcpy(copy_dst, copy_src, row_size);
    }

    const auto dest   = CD3DX12_TEXTURE_COPY_LOCATION(inResource.Get(), inSubResource);
    const auto source = CD3DX12_TEXTURE_COPY_LOCATION(buffer.GetResource().Get(), footprint);

    inCmdList->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);

    m_Buffers.emplace_back( StagingBuffer {
        .mSize      = total_size,
        .mCapacity  = total_size,
        .mRetired   = false,
        .mPtr       = mapped_ptr,
        .mBufferID  = buffer_id
    });

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(inResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ, inSubResource);
    inCmdList->ResourceBarrier(1, &barrier);
}



void RingAllocator::CreateBuffer(Device& inDevice, uint32_t inCapacity) {
    m_TotalCapacity = inCapacity;

    m_Buffer = inDevice.CreateBuffer(Buffer::Desc{
        .size  = inCapacity,
        .usage = Buffer::Usage::UPLOAD
    });

    auto buffer_range = CD3DX12_RANGE(0, 0);
    gThrowIfFailed(inDevice.GetBuffer(m_Buffer)->Map(0, &buffer_range, reinterpret_cast<void**>(&m_DataPtr)));
}



void RingAllocator::DestroyBuffer(Device& inDevice) {
    if (!m_Buffer.IsValid())
        return;

    inDevice.GetBuffer(m_Buffer)->Unmap(0, nullptr);
    inDevice.ReleaseBuffer(m_Buffer);
}



ComPtr<IDxcBlob> sCompileShaderDXC(const Path& inFilePath) {
    const auto name = inFilePath.stem().string();
    auto type = name.substr(name.size() - 2, 2);
    std::transform(type.begin(), type.end(), type.begin(), tolower);

    auto utils = ComPtr<IDxcUtils>{};
    auto library = ComPtr<IDxcLibrary>{};
    auto compiler = ComPtr<IDxcCompiler3>{};
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(utils.GetAddressOf()));
    DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(library.GetAddressOf()));
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(compiler.GetAddressOf()));

    auto include_handler = ComPtr<IDxcIncludeHandler>{};
    utils->CreateDefaultIncludeHandler(include_handler.GetAddressOf());

    auto ifs = std::ifstream(inFilePath);
    auto buffer = std::stringstream();
    buffer << ifs.rdbuf();
    const auto source_str = buffer.str();

    auto blob = ComPtr<IDxcBlobEncoding>();
    gThrowIfFailed(library->CreateBlobWithEncodingFromPinned(source_str.c_str(), source_str.size(), CP_UTF8, blob.GetAddressOf()));

    auto arguments = std::vector<LPCWSTR>{};
    arguments.push_back(L"-E");
    arguments.push_back(L"main");

    arguments.push_back(L"-T");

    if (type == "ps")
        arguments.push_back(L"ps_6_6");
    else if (type == "vs")
        arguments.push_back(L"vs_6_6");
    else if (type == "cs")
        arguments.push_back(L"cs_6_6");

    arguments.push_back(L"-Zi");
#ifndef NDEBUG
    arguments.push_back(L"-Qembed_debug");
    arguments.push_back(L"-Od");
#endif

    arguments.push_back(L"-I");
    arguments.push_back(L"assets/system/shaders/DirectX");

    arguments.push_back(L"-HV");
    arguments.push_back(L"2021");

    auto str_filepath = inFilePath.string();
    auto wstr_filepath = std::wstring(str_filepath.begin(), str_filepath.end());
    arguments.push_back(DXC_ARG_DEBUG_NAME_FOR_SOURCE);
    arguments.push_back(wstr_filepath.c_str());

    const auto source_buffer = DxcBuffer {
        .Ptr      = blob->GetBufferPointer(),
        .Size     = blob->GetBufferSize(),
        .Encoding = 0
    };

    auto result = ComPtr<IDxcResult>{};
    gThrowIfFailed(compiler->Compile(&source_buffer, arguments.data(), uint32_t(arguments.size()), include_handler.Get(), IID_PPV_ARGS(result.GetAddressOf())));

    auto hr_status = HRESULT{};
    result->GetStatus(&hr_status);

    auto errors = ComPtr<IDxcBlobUtf8>{};
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.GetAddressOf()), nullptr);

    if (errors && errors->GetStringLength() > 0) {
        auto lock = Async::sLock();
        auto error_c_str = static_cast<char*>(errors->GetBufferPointer());
        std::cout << error_c_str << '\n';

        auto error_str = std::string();
        auto line_nr = 0, char_nr = 0;

        auto token = strtok(error_c_str, ":"); 

        if (strncmp(token, "error", strlen("error")) != 0) {
            token = strtok(NULL, ":"); // File path/name is ignored
            line_nr = atoi(token);
            token = strtok(NULL, ":");
            char_nr = atoi(token);
            token = strtok(NULL, ":");
            error_str = std::string(token);
        }

        token = strtok(NULL, ":");
        const auto error_msg = std::string(token);

        OutputDebugStringA(std::format("{}({}): Error: {}", FileSystem::absolute(inFilePath).string(), line_nr, error_msg).c_str());

    }

    if (!SUCCEEDED(hr_status)) {
        std::cout << "Compilation " << COUT_RED("failed") << " for shader: " << inFilePath.string() << '\n';
        return nullptr;
    }

    auto shader = ComPtr<IDxcBlob>{}, pdb = ComPtr<IDxcBlob>{};
    auto debug_data = ComPtr<IDxcBlobUtf16>{};
    result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(shader.GetAddressOf()), debug_data.GetAddressOf());
    result->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(pdb.GetAddressOf()), debug_data.GetAddressOf());

    auto pdb_file = std::ofstream(inFilePath.parent_path() / inFilePath.filename().replace_extension(".pdb"));
    pdb_file.write((char*)pdb->GetBufferPointer(), pdb->GetBufferSize());

    auto lock = Async::sLock();
    std::cout << "Compilation " << COUT_GREEN("Finished") << " for shader: " << inFilePath.string() << '\n';
 
    return shader;
}

} // namespace::Raekor
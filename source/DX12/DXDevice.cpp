#include "pch.h"
#include "DXDevice.h"

#include "DXUtil.h"
#include "DXSampler.h"
#include "DXCommandList.h"
#include "DXRenderGraph.h"

#include "Raekor/OS.h"
#include "Raekor/timer.h"
#include "Raekor/async.h"

#include <locale>
#include <codecvt>

namespace Raekor::DX12 {

Device::Device(SDL_Window* window, uint32_t inFrameCount) : m_NumFrames(inFrameCount)
{
    auto device_creation_flags = 0u;

#ifndef NDEBUG
    auto debug_interface = ComPtr<ID3D12Debug1> {};

    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface))))
    {
        if (g_CVars.Create("debug_layer", 0))
            debug_interface->EnableDebugLayer();


        if (g_CVars.Create("gpu_validation", 0))
        {
            debug_interface->SetEnableGPUBasedValidation(TRUE);
            debug_interface->SetEnableSynchronizedCommandQueueValidation(TRUE);
        }
    }

    device_creation_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    if (OS::sCheckCommandLineOption("-hook_pix"))
    {
        auto pix_module = PIXLoadLatestWinPixGpuCapturerLibrary();
        assert(pix_module);
    }

    auto factory = ComPtr<IDXGIFactory6> {};
    gThrowIfFailed(CreateDXGIFactory2(device_creation_flags, IID_PPV_ARGS(&factory)));
    factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&m_Adapter));
    gThrowIfFailed(D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&m_Device)));
    
    auto temp_path = fs::temp_directory_path().wstring();

    NVSDK_NGX_Application_Identifier ngx_app_id = {};
    ngx_app_id.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Application_Id;
    ngx_app_id.v.ApplicationId = 0xdeadbeef;

    static const auto NGXLogCallback = [](const char* message, NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature sourceComponent) {
        std::cout << message << '\n';
    };

    NVSDK_NGX_FeatureCommonInfo ngx_common_info = {};
    ngx_common_info.LoggingInfo.LoggingCallback = NGXLogCallback;
    ngx_common_info.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_VERBOSE;
    ngx_common_info.LoggingInfo.DisableOtherLoggingSinks = true;

    NVSDK_NGX_FeatureDiscoveryInfo feature_info = {};
    feature_info.SDKVersion = NVSDK_NGX_Version_API;
    feature_info.FeatureID = NVSDK_NGX_Feature_SuperSampling;
    feature_info.Identifier = ngx_app_id;
    feature_info.ApplicationDataPath = temp_path.c_str();
    feature_info.FeatureInfo = &ngx_common_info;

    NVSDK_NGX_FeatureRequirement nv_supported = {};
    const auto ngx_result = NVSDK_NGX_D3D12_GetFeatureRequirements(m_Adapter.Get(), &feature_info, &nv_supported);

    if (ngx_result == NVSDK_NGX_Result::NVSDK_NGX_Result_Success && nv_supported.FeatureSupported == 0)
        mIsDLSSSupported = true;
    else if (ngx_result != NVSDK_NGX_Result_FAIL_FeatureNotSupported)
        gThrowIfFailed(ngx_result);

    gThrowIfFailed(NVSDK_NGX_D3D12_Init(0xdeadbeef, temp_path.c_str(), m_Device.Get()));

    const auto allocator_desc = D3D12MA::ALLOCATOR_DESC { .pDevice = m_Device.Get(), .pAdapter = m_Adapter.Get() };
    gThrowIfFailed(D3D12MA::CreateAllocator(&allocator_desc, &m_Allocator));

    constexpr auto copy_queue_desc = D3D12_COMMAND_QUEUE_DESC { .Type = D3D12_COMMAND_LIST_TYPE_COPY };
    constexpr auto direct_queue_desc = D3D12_COMMAND_QUEUE_DESC { .Type = D3D12_COMMAND_LIST_TYPE_DIRECT };
    constexpr auto compute_queue_desc = D3D12_COMMAND_QUEUE_DESC { .Type = D3D12_COMMAND_LIST_TYPE_COMPUTE };
    gThrowIfFailed(m_Device->CreateCommandQueue(&copy_queue_desc, IID_PPV_ARGS(&m_CopyQueue)));
    gThrowIfFailed(m_Device->CreateCommandQueue(&compute_queue_desc, IID_PPV_ARGS(&m_ComputeQueue)));
    gThrowIfFailed(m_Device->CreateCommandQueue(&direct_queue_desc, IID_PPV_ARGS(&m_GraphicsQueue)));
    
    // Since buffers/textures also correspond to a resource descriptor, we should / can never allocate more than the resource heap limit.
    // Because we reserve this upfront we also get stable pointers.
    m_Buffers.Reserve(sMaxResourceHeapSize);
    m_Textures.Reserve(sMaxResourceHeapSize);

    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]         = DescriptorHeap(*this, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, sMaxRTVHeapSize, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]         = DescriptorHeap(*this, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, sMaxDSVHeapSize, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]     = DescriptorHeap(*this, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, sMaxSamplerHeapSize, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = DescriptorHeap(*this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, sMaxResourceHeapSize, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

    for (auto sampler_index = 0u; sampler_index < ESamplerIndex::SAMPLER_COUNT; sampler_index++)
        m_Device->CreateSampler(&SAMPLER_DESC[sampler_index], m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].GetCPUDescriptorHandle(DescriptorID(sampler_index)));

    auto root_params = std::vector<D3D12_ROOT_PARAMETER1> {};
    auto b_registers = 0u, t_registers = 0u;

    root_params.emplace_back(D3D12_ROOT_PARAMETER1
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants = D3D12_ROOT_CONSTANTS {
                .ShaderRegister = b_registers++,
                .Num32BitValues = sMaxRootConstantsSize / (uint32_t)sizeof(DWORD),
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        });

    for (uint32_t bind_slot = EBindSlot::SRV0; bind_slot < EBindSlot::Count; bind_slot++)
    {
        auto param = D3D12_ROOT_PARAMETER1
        {
            .Descriptor = D3D12_ROOT_DESCRIPTOR1 {.RegisterSpace = 0 },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        };

        switch (bind_slot)
        {
            case EBindSlot::SRV0: case EBindSlot::SRV1:
                param.Descriptor.ShaderRegister = t_registers++;
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
                break;
            default: assert(false);
        }

        root_params.push_back(param);
    }


    auto vrsd = CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC {};
    vrsd.Init_1_1(
        root_params.size(), root_params.data(),
        ESamplerIndex::SAMPLER_COUNT, STATIC_SAMPLER_DESC.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
        D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
    );

    auto signature = ComPtr<ID3DBlob> {}, error = ComPtr<ID3DBlob> {};
    auto serialize_vrs_hr = D3DX12SerializeVersionedRootSignature(&vrsd, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);

    if (error)
        OutputDebugStringA((char*)error->GetBufferPointer());

    gThrowIfFailed(serialize_vrs_hr);
    gThrowIfFailed(m_Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_GlobalRootSignature)));

    gThrowIfFailed(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &mIsTearingSupported, sizeof(mIsTearingSupported)));

    /*ComPtr<ID3D12QueryHeap> m_TimestampQueryHeap;
    D3D12_QUERY_HEAP_DESC query_heap_desc = {};
    query_heap_desc.
    m_Device->CreateQueryHeap(&query_heap_desc, IID_PPV_ARGS(m_TimestampQueryHeap.GetAddressOf()));*/
}



void Device::BindDrawDefaults(CommandList& inCmdList)
{
    inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const auto heaps = std::array {
        *GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER),
        *GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    };

    inCmdList->SetDescriptorHeaps(heaps.size(), heaps.data());
    inCmdList->SetComputeRootSignature(GetGlobalRootSignature());
    inCmdList->SetGraphicsRootSignature(GetGlobalRootSignature());
}



TextureID Device::CreateTexture(const Texture::Desc& inDesc)
{
    auto texture = Texture(inDesc);
    auto resource_desc = D3D12_RESOURCE_DESC(CD3DX12_RESOURCE_DESC::Tex2D(inDesc.format, inDesc.width, inDesc.height, inDesc.arrayLayers, inDesc.mipLevels, 1u, 0, D3D12_RESOURCE_FLAG_NONE, D3D12_TEXTURE_LAYOUT_UNKNOWN));

    auto alloc_desc = D3D12MA::ALLOCATION_DESC {};
    alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    auto clear_value = D3D12_CLEAR_VALUE {};
    auto clear_value_ptr = static_cast<D3D12_CLEAR_VALUE*>( nullptr );

    auto initial_state = D3D12_RESOURCE_STATE_COMMON;

    switch (inDesc.usage)
    {
        case Texture::DEPTH_STENCIL_TARGET:
        {
            initial_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            alloc_desc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
            resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            clear_value = CD3DX12_CLEAR_VALUE(inDesc.format, 1.0f, 0.0f);
            clear_value_ptr = &clear_value;

            if (inDesc.viewDesc)
                clear_value.Format = static_cast<D3D12_DEPTH_STENCIL_VIEW_DESC*>( inDesc.viewDesc )->Format;
        } break;

        case Texture::RENDER_TARGET:
        {
            initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
            alloc_desc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
            resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

            float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            clear_value = CD3DX12_CLEAR_VALUE(inDesc.format, color);
            clear_value_ptr = &clear_value;

            if (inDesc.viewDesc)
                clear_value.Format = static_cast<D3D12_RENDER_TARGET_VIEW_DESC*>( inDesc.viewDesc )->Format;
        } break;

        case Texture::SHADER_READ_ONLY:
        {
            // According to D3D12 implicit state transitions, COMMON can be promoted to COPY_DEST or GENERIC_READ implicitly,
            // which is typically what you need for a texture that will only be read in a shader.
            initial_state = D3D12_RESOURCE_STATE_COMMON;
        } break;

        case Texture::SHADER_READ_WRITE:
        {
            initial_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        } break;
    }

    gThrowIfFailed(m_Allocator->CreateResource(&alloc_desc, &resource_desc, initial_state, clear_value_ptr, texture.m_Allocation.GetAddressOf(), IID_PPV_ARGS(&texture.m_Resource)));

    if (inDesc.debugName != nullptr)
        texture.m_Resource->SetName(inDesc.debugName);

    const auto texture_id = m_Textures.Add(texture);
    CreateDescriptor(texture_id, inDesc);

    return texture_id;
}



BufferID Device::CreateBuffer(const Buffer::Desc& inDesc)
{
    auto alloc_desc = D3D12MA::ALLOCATION_DESC {};
    alloc_desc.HeapType = inDesc.mappable ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;

    if (inDesc.usage & Buffer::Usage::READBACK)
        alloc_desc.HeapType = D3D12_HEAP_TYPE_READBACK;

    auto buffer = Buffer(inDesc);
    auto initial_state = D3D12_RESOURCE_STATE_COMMON;
    auto buffer_desc = D3D12_RESOURCE_DESC(CD3DX12_RESOURCE_DESC::Buffer(inDesc.size));

    switch (inDesc.usage)
    {
        case Buffer::VERTEX_BUFFER:
        {
            initial_state = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            if (!inDesc.mappable)
                buffer_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        } break;

        case Buffer::INDEX_BUFFER:
        {
            initial_state = D3D12_RESOURCE_STATE_INDEX_BUFFER;
            if (!inDesc.mappable)
                buffer_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        } break;

        case Buffer::UPLOAD:
        {
            alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
        } break;

        case Buffer::SHADER_READ_WRITE:
        {
            initial_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            buffer_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        } break;

        case Buffer::ACCELERATION_STRUCTURE:
        {
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

    if (inDesc.debugName != nullptr)
        buffer.m_Resource->SetName(inDesc.debugName);

    const auto buffer_id = m_Buffers.Add(buffer);
    CreateDescriptor(buffer_id, inDesc);

    return buffer_id;
}



BufferID Device::CreateBufferView(BufferID inBufferID, const Buffer::Desc& inDesc)
{
    // make a copy of the texture
    const auto& buffer = GetBuffer(inBufferID);
    auto new_buffer = Buffer(buffer);
    new_buffer.m_Desc.usage = inDesc.usage;
    new_buffer.m_Desc.viewDesc = inDesc.viewDesc;

    const auto buffer_id = m_Buffers.Add(new_buffer);
    CreateDescriptor(buffer_id, inDesc);

    return buffer_id;
}



TextureID Device::CreateTextureView(TextureID inTextureID, const Texture::Desc& inDesc)
{
    // make a copy of the texture
    const auto& texture = GetTexture(inTextureID);
    auto new_texture = Texture(texture);
    new_texture.m_Desc.usage = inDesc.usage;
    new_texture.m_Desc.viewDesc = inDesc.viewDesc;

    const auto texture_id = m_Textures.Add(new_texture);
    CreateDescriptor(texture_id, inDesc);

    return texture_id;
}



[[nodiscard]] TextureID Device::CreateTextureView(ResourceRef inResource, const Texture::Desc& inDesc)
{
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


void Device::ReleaseBufferDeferred(BufferID inID)
{
}



void Device::ReleaseBuffer(BufferID inBufferID)
{
    assert(inBufferID.IsValid());
    m_Buffers.Remove(inBufferID);
    ReleaseDescriptor(inBufferID);
}



void Device::ReleaseTexture(TextureID inTextureID)
{
    assert(inTextureID.IsValid());
    m_Textures.Remove(inTextureID);
    ReleaseDescriptor(inTextureID);
}



void Device::ReleaseBufferImmediate(BufferID inBufferID)
{
    assert(inBufferID.IsValid());

    auto& buffer = GetBuffer(inBufferID);
    buffer.m_Resource = nullptr;

    m_Buffers.Remove(inBufferID);
    // ReleaseDescriptorImmediate(inBufferID);
}



void Device::ReleaseTextureImmediate(TextureID inTextureID)
{
    assert(inTextureID.IsValid());

    auto& texture = GetTexture(inTextureID);
    texture.m_Resource = nullptr;

    m_Textures.Remove(inTextureID);
    ReleaseDescriptorImmediate(inTextureID);
}



D3D12_GRAPHICS_PIPELINE_STATE_DESC Device::CreatePipelineStateDesc(IRenderPass* inRenderPass, const CD3DX12_SHADER_BYTECODE& inVertexShader, const CD3DX12_SHADER_BYTECODE& inPixelShader)
{
    assert(inRenderPass->IsGraphics() && "Cannot create a Graphics PSO description for a Compute RenderPass");

    static constexpr auto vertex_layout = std::array
    {
        D3D12_INPUT_ELEMENT_DESC { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    auto pso_state = D3D12_GRAPHICS_PIPELINE_STATE_DESC
    {
        .pRootSignature = GetGlobalRootSignature(),
        .VS = inVertexShader,
        .PS = inPixelShader,
        .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
        .SampleMask = UINT_MAX,
        .RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
        .DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
        .InputLayout = D3D12_INPUT_LAYOUT_DESC
        {
            .pInputElementDescs = vertex_layout.data(),
            .NumElements = vertex_layout.size(),
        },
        .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .SampleDesc = DXGI_SAMPLE_DESC {.Count = 1 },
    };

    pso_state.RasterizerState.FrontCounterClockwise = TRUE;

    for (auto format : inRenderPass->m_RenderTargetFormats)
    {
        if (format == DXGI_FORMAT_UNKNOWN)
            break;

        pso_state.RTVFormats[pso_state.NumRenderTargets++] = format;
    }

    pso_state.DSVFormat = inRenderPass->m_DepthStencilFormat;

    return pso_state;
}



D3D12_COMPUTE_PIPELINE_STATE_DESC Device::CreatePipelineStateDesc(IRenderPass* inRenderPass, const CD3DX12_SHADER_BYTECODE& inComputeShader)
{
    assert(inRenderPass->IsCompute() && "Cannot create a Compute PSO description for a GraphicsRenderPass");
    return D3D12_COMPUTE_PIPELINE_STATE_DESC { .pRootSignature = GetGlobalRootSignature(), .CS = inComputeShader };
}



void Device::CreateDescriptor(BufferID inID, const Buffer::Desc& inDesc)
{
    auto& buffer = GetBuffer(inID);
    buffer.m_Desc.usage = inDesc.usage;
    buffer.m_Desc.viewDesc = inDesc.viewDesc;

    switch (inDesc.usage)
    {
        case Buffer::SHADER_READ_WRITE:
        {
            if (inDesc.viewDesc == nullptr)
            {
                auto desc = D3D12_UNORDERED_ACCESS_VIEW_DESC {};
                desc.Format = inDesc.format;
                desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

                // Raw buffer
                if (inDesc.stride == 0 && desc.Format == DXGI_FORMAT_UNKNOWN)
                {
                    desc.Format = DXGI_FORMAT_R32_TYPELESS;
                    desc.Buffer.Flags |= D3D12_BUFFER_UAV_FLAG_RAW;
                    desc.Buffer.NumElements = inDesc.size / sizeof(uint32_t);
                }
                // Structured buffer
                else if (inDesc.stride > 0) 
                {
                    desc.Buffer.StructureByteStride = inDesc.stride;
                    desc.Buffer.NumElements = inDesc.size / inDesc.stride;
                }

                buffer.m_Descriptor = CreateUnorderedAccessView(buffer.m_Resource, &desc);
            }
            else
                buffer.m_Descriptor = CreateUnorderedAccessView(buffer.m_Resource, static_cast<D3D12_UNORDERED_ACCESS_VIEW_DESC*>( inDesc.viewDesc ));
        } break;

        case Buffer::ACCELERATION_STRUCTURE:
        {
            if (inDesc.viewDesc)
                buffer.m_Descriptor = CreateShaderResourceView(nullptr, static_cast<D3D12_SHADER_RESOURCE_VIEW_DESC*>( inDesc.viewDesc ));
            else
            {
                D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
                srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
                srv_desc.RaytracingAccelerationStructure.Location = buffer->GetGPUVirtualAddress();

                buffer.m_Descriptor = CreateShaderResourceView(nullptr, &srv_desc);
            }
        } break;

        case Buffer::UPLOAD:
        case Buffer::GENERAL:
        case Buffer::INDEX_BUFFER:
        case Buffer::VERTEX_BUFFER:
        case Buffer::SHADER_READ_ONLY:
        {
            if (inDesc.viewDesc)
                buffer.m_Descriptor = CreateShaderResourceView(buffer.m_Resource, static_cast<D3D12_SHADER_RESOURCE_VIEW_DESC*>( inDesc.viewDesc ));
            else if (inDesc.stride)
            {
                D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
                srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                srv_desc.Buffer.StructureByteStride = inDesc.stride;
                srv_desc.Buffer.NumElements = inDesc.size / inDesc.stride;

                buffer.m_Descriptor = CreateShaderResourceView(buffer.m_Resource, &srv_desc);

            }
        } break;

        default:
            assert(false); // should not be able to get here
    }
}



void Device::CreateDescriptor(TextureID inID, const Texture::Desc& inDesc)
{
    auto& texture = GetTexture(inID);
    texture.m_Desc.usage = inDesc.usage;
    texture.m_Desc.viewDesc = inDesc.viewDesc;

    switch (inDesc.usage)
    {
        case Texture::DEPTH_STENCIL_TARGET:
        {
            assert(gIsDepthFormat(texture.m_Desc.format));
            texture.m_Descriptor = CreateDepthStencilView(texture.m_Resource, static_cast<D3D12_DEPTH_STENCIL_VIEW_DESC*>( inDesc.viewDesc ));
            break;
        }
        case Texture::RENDER_TARGET:
        {
            texture.m_Descriptor = CreateRenderTargetView(texture.m_Resource, static_cast<D3D12_RENDER_TARGET_VIEW_DESC*>( inDesc.viewDesc ));
            break;
        }
        case Texture::SHADER_READ_ONLY:
        {
            if (inDesc.viewDesc == nullptr)
            {
                const auto [r, g, b, a] = gUnswizzleComponents(inDesc.swizzle);

                D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
                srv_desc.Format = gGetDepthFormatSRV(texture.m_Desc.format);
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srv_desc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(r, g, b, a);
                srv_desc.Texture2D.MipLevels = -1;

                texture.m_Descriptor = CreateShaderResourceView(texture.m_Resource, &srv_desc);
            }
            else
                texture.m_Descriptor = CreateShaderResourceView(texture.m_Resource, static_cast<D3D12_SHADER_RESOURCE_VIEW_DESC*>( inDesc.viewDesc ));

            break;
        }
        case Texture::SHADER_READ_WRITE:
        {
            texture.m_Descriptor = CreateUnorderedAccessView(texture.m_Resource, static_cast<D3D12_UNORDERED_ACCESS_VIEW_DESC*>( inDesc.viewDesc ));
            break;
        }
        default:
            assert(false); // should not be able to get here
    }
}


void Device::ReleaseDescriptor(BufferID inBufferID)
{
    auto& buffer = GetBuffer(inBufferID);
    m_Heaps[gGetHeapType(buffer.m_Desc.usage)].Remove(buffer.m_Descriptor);
}


void Device::ReleaseDescriptor(TextureID inTextureID)
{
    auto& texture = GetTexture(inTextureID);
    m_Heaps[gGetHeapType(texture.m_Desc.usage)].Remove(texture.m_Descriptor);
}


void Device::ReleaseDescriptorImmediate(BufferID inBufferID)
{
    auto& buffer = GetBuffer(inBufferID);
    m_Heaps[gGetHeapType(buffer.m_Desc.usage)].Get(buffer.m_Descriptor) = nullptr;
    ReleaseDescriptor(inBufferID);
}


void Device::ReleaseDescriptorImmediate(TextureID inTextureID)
{
    auto& texture = GetTexture(inTextureID);
    m_Heaps[gGetHeapType(texture.m_Desc.usage)].Get(texture.m_Descriptor) = nullptr;
    ReleaseDescriptor(inTextureID);
}



D3D12_CPU_DESCRIPTOR_HANDLE Device::GetCPUDescriptorHandle(TextureID inID)
{
    auto& texture = GetTexture(inID);

    switch (texture.GetDesc().usage)
    {
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
    return CD3DX12_CPU_DESCRIPTOR_HANDLE {};
}



D3D12_CPU_DESCRIPTOR_HANDLE Device::GetCPUDescriptorHandle(BufferID inID)
{
    auto& buffer = GetBuffer(inID);
    return GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetCPUDescriptorHandle(buffer.GetDescriptor());
}



D3D12_GPU_DESCRIPTOR_HANDLE Device::GetGPUDescriptorHandle(BufferID inID)
{
    auto& buffer = GetBuffer(inID);
    return GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetGPUDescriptorHandle(buffer.GetDescriptor());
}



D3D12_GPU_DESCRIPTOR_HANDLE Device::GetGPUDescriptorHandle(TextureID inID)
{
    auto& texture = GetTexture(inID);
    return GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetGPUDescriptorHandle(texture.GetView());
}



DescriptorID Device::CreateDepthStencilView(ResourceRef inResource, const D3D12_DEPTH_STENCIL_VIEW_DESC* inDesc)
{
    auto& heap = m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV];
    const auto descriptor_id = heap.Add(inResource);
    m_Device->CreateDepthStencilView(inResource.Get(), inDesc, heap.GetCPUDescriptorHandle(descriptor_id));
    return descriptor_id;
}



DescriptorID Device::CreateRenderTargetView(ResourceRef inResource, const D3D12_RENDER_TARGET_VIEW_DESC* inDesc)
{
    auto& heap = m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
    const auto descriptor_id = heap.Add(inResource);
    Timer timer;
    m_Device->CreateRenderTargetView(inResource.Get(), inDesc, heap.GetCPUDescriptorHandle(descriptor_id));
    return descriptor_id;
}



DescriptorID Device::CreateShaderResourceView(ResourceRef inResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* inDesc)
{
    auto& heap = m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    const auto descriptor_id = heap.Add(inResource);
    m_Device->CreateShaderResourceView(inResource.Get(), inDesc, heap.GetCPUDescriptorHandle(descriptor_id));
    return descriptor_id;
}



DescriptorID Device::CreateUnorderedAccessView(ResourceRef inResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* inDesc)
{
    auto& heap = m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    const auto descriptor_id = heap.Add(inResource);
    m_Device->CreateUnorderedAccessView(inResource.Get(), nullptr, inDesc, heap.GetCPUDescriptorHandle(descriptor_id));
    return descriptor_id;
}



StagingHeap::~StagingHeap()
{
    for (auto& buffer_view : m_Buffers)
        m_Device.GetBuffer(buffer_view.mBufferID)->Unmap(0, nullptr);
}



void StagingHeap::StageBuffer(CommandList& inCmdList, ResourceRef inResource, uint32_t inOffset, const void* inData, uint32_t inSize)
{
    for (auto& buffer : m_Buffers)
    {
        if (buffer.mRetired && inSize < buffer.mCapacity - buffer.mSize)
        {
            memcpy(buffer.mPtr + buffer.mSize, inData, inSize);

            const auto buffer_resource = m_Device.GetBuffer(buffer.mBufferID).GetResource();
            inCmdList->CopyBufferRegion(inResource.Get(), inOffset, buffer_resource.Get(), buffer.mSize, inSize);

            buffer.mSize += inSize;
            buffer.mRetired = false;
            return;
        }
    }

    auto buffer_id = m_Device.CreateBuffer(Buffer::Desc
    {
        .size = inSize,
        .usage = Buffer::Usage::UPLOAD
    });

    auto& buffer = m_Device.GetBuffer(buffer_id);

    auto mapped_ptr = static_cast<uint8_t*>( nullptr );
    const auto range = CD3DX12_RANGE(0, 0);
    gThrowIfFailed(buffer->Map(0, &range, reinterpret_cast<void**>( &mapped_ptr )));

    memcpy(mapped_ptr, inData, inSize);

    assert(inResource->GetDesc().Width >= inSize);
    inCmdList->CopyBufferRegion(inResource.Get(), inOffset, buffer.GetResource().Get(), 0, inSize);

    m_Buffers.emplace_back(StagingBuffer
    {
        .mSize = inSize,
        .mCapacity = inSize,
        .mFenceValue = inCmdList.GetFenceValue(),
        .mRetired = false,
        .mPtr = mapped_ptr,
        .mBufferID = buffer_id
    });
}



void StagingHeap::StageTexture(CommandList& inCmdList, ResourceRef inResource, uint32_t inSubResource, const void* inData)
{
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

    auto buffer_id = m_Device.CreateBuffer(Buffer::Desc {
        .size = uint32_t(total_size),
        .usage = Buffer::Usage::UPLOAD
    });

    auto& buffer = m_Device.GetBuffer(buffer_id);

    auto mapped_ptr = static_cast<uint8_t*>( nullptr );
    const auto range = CD3DX12_RANGE(0, 0);
    gThrowIfFailed(buffer->Map(0, &range, reinterpret_cast<void**>( &mapped_ptr )));

    mapped_ptr += footprint.Offset;
    auto data_ptr = static_cast<const uint8_t*>( inData );

    for (uint32_t row = 0u; row < nr_of_rows; row++)
    {
        const auto copy_src = data_ptr + row * row_size;
        const auto copy_dst = mapped_ptr + row * aligned_row_size;
        memcpy(copy_dst, copy_src, row_size);
    }

    const auto dest = CD3DX12_TEXTURE_COPY_LOCATION(inResource.Get(), inSubResource);
    const auto source = CD3DX12_TEXTURE_COPY_LOCATION(buffer.GetResource().Get(), footprint);

    inCmdList->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);

    m_Buffers.emplace_back(StagingBuffer
    {
        .mSize = total_size,
        .mCapacity = total_size,
        .mFenceValue = inCmdList.GetFenceValue(),
        .mRetired = false,
        .mPtr = mapped_ptr,
        .mBufferID = buffer_id
    });

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(inResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ, inSubResource);
    inCmdList->ResourceBarrier(1, &barrier);
}



void RingAllocator::CreateBuffer(Device& inDevice, uint32_t inCapacity)
{
    m_TotalCapacity = inCapacity;

    m_Buffer = inDevice.CreateBuffer(Buffer::Desc {.size = inCapacity, .usage = Buffer::Usage::UPLOAD });

    auto buffer_range = CD3DX12_RANGE(0, 0);
    gThrowIfFailed(inDevice.GetBuffer(m_Buffer)->Map(0, &buffer_range, reinterpret_cast<void**>( &m_DataPtr )));
}



void RingAllocator::DestroyBuffer(Device& inDevice)
{
    if (!m_Buffer.IsValid())
        return;

    inDevice.GetBuffer(m_Buffer)->Unmap(0, nullptr);
    inDevice.ReleaseBuffer(m_Buffer);
}


} // namespace::Raekor
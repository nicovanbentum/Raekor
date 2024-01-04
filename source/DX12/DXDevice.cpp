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
    uint32_t device_creation_flags = 0u;

#ifndef NDEBUG
    auto debug_interface = ComPtr<ID3D12Debug1> {};

    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface))))
    {
        static auto debug_layer_enabled = OS::sCheckCommandLineOption("-debug_layer");
        static auto gpu_validation_enabled = OS::sCheckCommandLineOption("-gpu_validation");

        if (debug_layer_enabled)
            debug_interface->EnableDebugLayer();

        if (gpu_validation_enabled)
        {
            debug_interface->SetEnableGPUBasedValidation(TRUE);
            debug_interface->SetEnableSynchronizedCommandQueueValidation(TRUE);
        }
    }

    device_creation_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    if (OS::sCheckCommandLineOption("-hook_pix"))
    {
        HMODULE pix_module = PIXLoadLatestWinPixGpuCapturerLibrary();
        assert(pix_module);
    }

    ComPtr<IDXGIFactory6> factory = nullptr;
    gThrowIfFailed(CreateDXGIFactory2(device_creation_flags, IID_PPV_ARGS(&factory)));

    factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&m_Adapter));

    gThrowIfFailed(D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&m_Device)));
    
    const static bool enable_dlss = OS::sCheckCommandLineOption("-enable_dlss");
    if (enable_dlss)
    {
        std::wstring temp_path = fs::temp_directory_path().wstring();

        NVSDK_NGX_Application_Identifier ngx_app_id = {};
        ngx_app_id.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Application_Id;
        ngx_app_id.v.ApplicationId = 0xdeadbeef;

        static const auto NGXLogCallback = [](const char* message, NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature sourceComponent) 
        {
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
        const NVSDK_NGX_Result ngx_result = NVSDK_NGX_D3D12_GetFeatureRequirements(m_Adapter.Get(), &feature_info, &nv_supported);

        if (ngx_result == NVSDK_NGX_Result::NVSDK_NGX_Result_Success && nv_supported.FeatureSupported == 0)
            mIsDLSSSupported = true;
        else if (ngx_result != NVSDK_NGX_Result_FAIL_FeatureNotSupported)
            gThrowIfFailed(ngx_result);

        gThrowIfFailed(NVSDK_NGX_D3D12_Init(0xdeadbeef, temp_path.c_str(), m_Device.Get()));
    }

    const D3D12MA::ALLOCATOR_DESC allocator_desc = D3D12MA::ALLOCATOR_DESC { .pDevice = m_Device.Get(), .pAdapter = m_Adapter.Get() };
    gThrowIfFailed(D3D12MA::CreateAllocator(&allocator_desc, &m_Allocator));

    constexpr D3D12_COMMAND_QUEUE_DESC copy_queue_desc = D3D12_COMMAND_QUEUE_DESC { .Type = D3D12_COMMAND_LIST_TYPE_COPY };
    constexpr D3D12_COMMAND_QUEUE_DESC direct_queue_desc = D3D12_COMMAND_QUEUE_DESC { .Type = D3D12_COMMAND_LIST_TYPE_DIRECT };
    constexpr D3D12_COMMAND_QUEUE_DESC compute_queue_desc = D3D12_COMMAND_QUEUE_DESC { .Type = D3D12_COMMAND_LIST_TYPE_COMPUTE };
    gThrowIfFailed(m_Device->CreateCommandQueue(&copy_queue_desc, IID_PPV_ARGS(&m_CopyQueue)));
    gThrowIfFailed(m_Device->CreateCommandQueue(&compute_queue_desc, IID_PPV_ARGS(&m_ComputeQueue)));
    gThrowIfFailed(m_Device->CreateCommandQueue(&direct_queue_desc, IID_PPV_ARGS(&m_GraphicsQueue)));
    
    // Since buffers/textures also correspond to a resource descriptor, we should / can never allocate more than the resource heap limit.
    // Because we reserve this upfront we also get stable pointers.
    m_Buffers.Reserve(sMaxResourceHeapSize);
    m_Textures.Reserve(sMaxResourceHeapSize);

    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Allocate(*this, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, sMaxRTVHeapSize, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Allocate(*this, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, sMaxDSVHeapSize, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].Allocate(*this, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, sMaxSamplerHeapSize, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Allocate(*this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, sMaxResourceHeapSize, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

    for (uint32_t sampler_index = 0u; sampler_index < ESamplerIndex::SAMPLER_COUNT; sampler_index++)
        m_Device->CreateSampler(&SAMPLER_DESC[sampler_index], m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].GetCPUDescriptorHandle(DescriptorID(sampler_index)));

    uint32_t b_registers = 0u, t_registers = 0u;
    std::vector<D3D12_ROOT_PARAMETER1> root_params;

    root_params.emplace_back(D3D12_ROOT_PARAMETER1
    {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
        .Constants = D3D12_ROOT_CONSTANTS 
        {
            .ShaderRegister = b_registers++,
            .Num32BitValues = sMaxRootConstantsSize / (uint32_t)sizeof(DWORD),
        },
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
    });

    for (uint32_t bind_slot = EBindSlot::Start; bind_slot < EBindSlot::Count; bind_slot++)
    {
        D3D12_ROOT_PARAMETER1 param = D3D12_ROOT_PARAMETER1
        {
            .Descriptor = D3D12_ROOT_DESCRIPTOR1 {.RegisterSpace = 0 },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        };

        switch (bind_slot)
        {
            case EBindSlot::CBV0:
                param.Descriptor.ShaderRegister = b_registers++;
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
                break;
            
            case EBindSlot::SRV0: case EBindSlot::SRV1:
                param.Descriptor.ShaderRegister = t_registers++;
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
                break;

            default: assert(false);
        }

        root_params.push_back(param);
    }


    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc = {};
    root_signature_desc.Init_1_1(
        root_params.size(), root_params.data(),
        ESamplerIndex::SAMPLER_COUNT, STATIC_SAMPLER_DESC.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
        D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
    );

    ComPtr<ID3DBlob> signature, error;
    HRESULT serialize_vrs_hr = D3DX12SerializeVersionedRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);

    if (error)
        OutputDebugStringA((char*)error->GetBufferPointer());

    gThrowIfFailed(serialize_vrs_hr);
    gThrowIfFailed(m_Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_GlobalRootSignature)));

    gThrowIfFailed(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &mIsTearingSupported, sizeof(mIsTearingSupported)));

    for (uint32_t cmd_sig = 0; cmd_sig < COMMAND_SIGNATURE_COUNT; cmd_sig++)
    {
        constexpr auto sizes = std::array { uint32_t(sizeof(D3D12_DRAW_ARGUMENTS)), uint32_t(sizeof(D3D12_DRAW_INDEXED_ARGUMENTS)), uint32_t(sizeof(D3D12_DISPATCH_ARGUMENTS)) };
        static_assert( sizes.size() == COMMAND_SIGNATURE_COUNT );

        auto indirect_args = D3D12_INDIRECT_ARGUMENT_DESC { .Type = (D3D12_INDIRECT_ARGUMENT_TYPE)cmd_sig };

        const auto cmdsig_desc = D3D12_COMMAND_SIGNATURE_DESC
        {
            .ByteStride = sizes[cmd_sig], .NumArgumentDescs = 1, .pArgumentDescs = &indirect_args
        };

        m_Device->CreateCommandSignature(&cmdsig_desc, nullptr, IID_PPV_ARGS(m_CommandSignatures[cmd_sig].GetAddressOf()));
    }

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
    Texture texture = Texture(inDesc);
    D3D12_RESOURCE_DESC resource_desc = {};

    switch (inDesc.dimension)
    {
        case Texture::TEX_DIM_2D:
            resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(inDesc.format, inDesc.width, inDesc.height, inDesc.depthOrArrayLayers, inDesc.mipLevels, 1u, 0);
            break;
        case Texture::TEX_DIM_3D:
            resource_desc = CD3DX12_RESOURCE_DESC::Tex3D(inDesc.format, inDesc.width, inDesc.height, inDesc.depthOrArrayLayers, inDesc.mipLevels);
            break;
        case Texture::TEX_DIM_CUBE:
            resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(inDesc.format, inDesc.width, inDesc.height, inDesc.depthOrArrayLayers * 6, inDesc.mipLevels, 1u, 0);
            break;
        default:
            assert(false);
    }

    D3D12MA::ALLOCATION_DESC alloc_desc = {};
    alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clear_value = {};
    D3D12_CLEAR_VALUE* clear_value_ptr = nullptr;
    D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;

    switch (inDesc.usage)
    {
        case Texture::DEPTH_STENCIL_TARGET:
        {
            initial_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            alloc_desc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
            resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            clear_value = CD3DX12_CLEAR_VALUE(inDesc.format, 1.0f, 0.0f);
            clear_value_ptr = &clear_value;
        } break;

        case Texture::RENDER_TARGET:
        {
            initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
            alloc_desc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
            resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

            float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            clear_value = CD3DX12_CLEAR_VALUE(inDesc.format, color);
            clear_value_ptr = &clear_value;
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
            resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        } break;
    }

    gThrowIfFailed(m_Allocator->CreateResource(&alloc_desc, &resource_desc, initial_state, clear_value_ptr, texture.m_Allocation.GetAddressOf(), IID_PPV_ARGS(&texture.m_Resource)));

    if (inDesc.debugName != nullptr)
        gSetDebugName(texture.m_Resource.Get(), inDesc.debugName);

    const TextureID texture_id = m_Textures.Add(texture);

    CreateDescriptor(texture_id, inDesc);

    return texture_id;
}



BufferID Device::CreateBuffer(const Buffer::Desc& inDesc)
{
    D3D12MA::ALLOCATION_DESC alloc_desc = {};
    alloc_desc.HeapType = inDesc.mappable ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;

    if (inDesc.usage & Buffer::Usage::READBACK)
        alloc_desc.HeapType = D3D12_HEAP_TYPE_READBACK;

    Buffer buffer = Buffer(inDesc);
    D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_DESC buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(inDesc.size);

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

    gThrowIfFailed(m_Allocator->CreateResource(&alloc_desc, &buffer_desc, initial_state, nullptr, buffer.m_Allocation.GetAddressOf(), IID_PPV_ARGS(&buffer.m_Resource)));

    if (inDesc.debugName != nullptr)
        gSetDebugName(buffer.m_Resource.Get(), inDesc.debugName);

    const BufferID buffer_id = m_Buffers.Add(buffer);

    CreateDescriptor(buffer_id, inDesc);

    return buffer_id;
}



BufferID Device::CreateBufferView(BufferID inBufferID, const Buffer::Desc& inDesc)
{
    // make a copy of the texture, super lightweight as the underlying resource is ref counted
    const Buffer& buffer = GetBuffer(inBufferID);
    Buffer new_buffer = Buffer(buffer);
    new_buffer.m_Desc.usage = inDesc.usage;

    const BufferID buffer_id = m_Buffers.Add(new_buffer);
    CreateDescriptor(buffer_id, inDesc);

    return buffer_id;
}



TextureID Device::CreateTextureView(TextureID inTextureID, const Texture::Desc& inDesc)
{
    // make a copy of the texture
    const Texture& texture = GetTexture(inTextureID);
    Texture new_texture = Texture(texture);
    new_texture.m_Desc.usage = inDesc.usage;

    const TextureID texture_id = m_Textures.Add(new_texture);
    CreateDescriptor(texture_id, inDesc);

    return texture_id;
}



[[nodiscard]] TextureID Device::CreateTextureView(D3D12ResourceRef inResource, const Texture::Desc& inDesc)
{
    Texture temp_texture = Texture(inDesc);
    temp_texture.m_Resource = inResource;
    const TextureID texture_id = m_Textures.Add(temp_texture);

    Texture& texture = GetTexture(texture_id);
    texture.m_Desc.format = texture.GetD3D12Resource()->GetDesc().Format;
    texture.m_Desc.usage = inDesc.usage;
    CreateDescriptor(texture_id, inDesc);

    return texture_id;
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

    Buffer& buffer = GetBuffer(inBufferID);
    buffer.m_Resource = nullptr;
    buffer.m_Allocation = nullptr;

    m_Buffers.Remove(inBufferID);
    ReleaseDescriptorImmediate(inBufferID);
}



void Device::ReleaseTextureImmediate(TextureID inTextureID)
{
    assert(inTextureID.IsValid());

    Texture& texture = GetTexture(inTextureID);
    texture.m_Resource = nullptr;
    texture.m_Allocation = nullptr;

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

    for (DXGI_FORMAT format : inRenderPass->m_RenderTargetFormats)
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
    assert(inRenderPass ? inRenderPass->IsCompute() : true && "Cannot create a Compute PSO description for a GraphicsRenderPass");
    return D3D12_COMPUTE_PIPELINE_STATE_DESC { .pRootSignature = GetGlobalRootSignature(), .CS = inComputeShader };
}



void Device::CreateDescriptor(BufferID inID, const Buffer::Desc& inDesc)
{
    Buffer& buffer = GetBuffer(inID);
    buffer.m_Desc.usage = inDesc.usage;

    switch (inDesc.usage)
    {
        case Buffer::SHADER_READ_WRITE:
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC desc = { .Format = inDesc.format, .ViewDimension = D3D12_UAV_DIMENSION_BUFFER };

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
            else
            {
                desc.Buffer.NumElements = inDesc.size / ( gBitsPerPixel(desc.Format) / 8 );
            }

            buffer.m_Descriptor = CreateUnorderedAccessView(buffer.m_Resource, &desc);
        } break;

        case Buffer::ACCELERATION_STRUCTURE:
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
            srv_desc.RaytracingAccelerationStructure.Location = buffer->GetGPUVirtualAddress();

            buffer.m_Descriptor = CreateShaderResourceView(nullptr, &srv_desc);
        } break;

        case Buffer::UPLOAD:
        case Buffer::GENERAL:
        case Buffer::INDEX_BUFFER:
        case Buffer::VERTEX_BUFFER:
        case Buffer::SHADER_READ_ONLY:
        case Buffer::INDIRECT_ARGUMENTS:
        {
            if (inDesc.stride)
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

    switch (inDesc.usage)
    {
        case Texture::DEPTH_STENCIL_TARGET:
        {
            assert(gIsDepthFormat(texture.m_Desc.format));
            texture.m_Descriptor = CreateDepthStencilView(texture.m_Resource, nullptr);
        } break;

        case Texture::RENDER_TARGET:
        {
            D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
            rtv_desc.Format = inDesc.format;
            rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

            if (inDesc.dimension == Texture::Dimension::TEX_DIM_2D)
            {
                rtv_desc.Texture2D.MipSlice = inDesc.baseMip;
                texture.m_Descriptor = CreateRenderTargetView(texture.m_Resource, &rtv_desc);
            }
        } break;

        case Texture::SHADER_READ_ONLY:
        {
            const auto [r, g, b, a] = gUnswizzleComponents(inDesc.swizzle);

            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.Format = gGetDepthFormatSRV(texture.m_Desc.format);
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(r, g, b, a);
            srv_desc.Texture2D.MipLevels = -1;

            switch (inDesc.dimension)
            {
                case Texture::TEX_DIM_CUBE:
                {
                    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                    srv_desc.TextureCube.MipLevels = -1;
                    srv_desc.TextureCube.MostDetailedMip = 0;
                    srv_desc.TextureCube.ResourceMinLODClamp = 0.0f;
                } break;
            }

            texture.m_Descriptor = CreateShaderResourceView(texture.m_Resource, &srv_desc);
        } break;

        case Texture::SHADER_READ_WRITE:
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
            uav_desc.Format = inDesc.format;
            uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uav_desc.Texture2D.MipSlice = inDesc.baseMip;

            texture.m_Descriptor = CreateUnorderedAccessView(texture.m_Resource, &uav_desc);
        } break;

        default:
            assert(false); // should not be able to get here
    }
}


void Device::ReleaseDescriptor(BufferID inBufferID)
{
    Buffer& buffer = GetBuffer(inBufferID);
    m_Heaps[gGetHeapType(buffer.m_Desc.usage)].Remove(buffer.m_Descriptor);
}


void Device::ReleaseDescriptor(TextureID inTextureID)
{
    Texture& texture = GetTexture(inTextureID);
    m_Heaps[gGetHeapType(texture.m_Desc.usage)].Remove(texture.m_Descriptor);
}


void Device::ReleaseDescriptorImmediate(BufferID inBufferID)
{
    Buffer& buffer = GetBuffer(inBufferID);
    m_Heaps[gGetHeapType(buffer.m_Desc.usage)].Get(buffer.m_Descriptor) = nullptr;
    ReleaseDescriptor(inBufferID);
}


void Device::ReleaseDescriptorImmediate(TextureID inTextureID)
{
    Texture& texture = GetTexture(inTextureID);
    m_Heaps[gGetHeapType(texture.m_Desc.usage)].Get(texture.m_Descriptor) = nullptr;
    ReleaseDescriptor(inTextureID);
}



D3D12_CPU_DESCRIPTOR_HANDLE Device::GetCPUDescriptorHandle(TextureID inID)
{
    Texture& texture = GetTexture(inID);

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
    Buffer& buffer = GetBuffer(inID);
    return GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetCPUDescriptorHandle(buffer.GetDescriptor());
}



D3D12_GPU_DESCRIPTOR_HANDLE Device::GetGPUDescriptorHandle(BufferID inID)
{
    Buffer& buffer = GetBuffer(inID);
    return GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetGPUDescriptorHandle(buffer.GetDescriptor());
}



D3D12_GPU_DESCRIPTOR_HANDLE Device::GetGPUDescriptorHandle(TextureID inID)
{
    Texture& texture = GetTexture(inID);
    return GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetGPUDescriptorHandle(texture.GetView());
}



DescriptorID Device::CreateDepthStencilView(D3D12ResourceRef inResource, const D3D12_DEPTH_STENCIL_VIEW_DESC* inDesc)
{
    DescriptorHeap& heap = m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV];
    DescriptorID descriptor = heap.Add(inResource);
    m_Device->CreateDepthStencilView(inResource.Get(), inDesc, heap.GetCPUDescriptorHandle(descriptor));
    return descriptor;
}



DescriptorID Device::CreateRenderTargetView(D3D12ResourceRef inResource, const D3D12_RENDER_TARGET_VIEW_DESC* inDesc)
{
    assert(inResource->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

    DescriptorHeap& heap = m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
    DescriptorID descriptor = heap.Add(inResource);
    Timer timer;
    m_Device->CreateRenderTargetView(inResource.Get(), inDesc, heap.GetCPUDescriptorHandle(descriptor));
    return descriptor;
}



DescriptorID Device::CreateShaderResourceView(D3D12ResourceRef inResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* inDesc)
{
    DescriptorHeap& heap = m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    DescriptorID descriptor = heap.Add(inResource);
    m_Device->CreateShaderResourceView(inResource.Get(), inDesc, heap.GetCPUDescriptorHandle(descriptor));
    return descriptor;
}



DescriptorID Device::CreateUnorderedAccessView(D3D12ResourceRef inResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* inDesc)
{
    DescriptorHeap& heap = m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    DescriptorID descriptor = heap.Add(inResource);
    m_Device->CreateUnorderedAccessView(inResource.Get(), nullptr, inDesc, heap.GetCPUDescriptorHandle(descriptor));
    return descriptor;
}



StagingHeap::~StagingHeap()
{
    for (StagingBuffer& buffer : m_Buffers)
        m_Device.GetBuffer(buffer.mBufferID)->Unmap(0, nullptr);
}



void StagingHeap::StageBuffer(CommandList& inCmdList, const Buffer& inBuffer, uint32_t inOffset, const void* inData, uint32_t inSize)
{
    inCmdList.TrackResource(inBuffer);

    for (StagingBuffer& buffer : m_Buffers)
    {
        if (buffer.mRetired && inSize <= buffer.mCapacity - buffer.mSize)
        {
            memcpy(buffer.mPtr + buffer.mSize, inData, inSize);

            ID3D12Resource* buffer_resource = m_Device.GetD3D12Resource(buffer.mBufferID);
            inCmdList->CopyBufferRegion(inBuffer.GetD3D12Resource().Get(), inOffset, buffer_resource, buffer.mSize, inSize);

            buffer.mSize += inSize;
            buffer.mRetired = false;
            buffer.mFrameIndex = inCmdList.GetFrameIndex();

            return;
        }
    }

    BufferID buffer_id = m_Device.CreateBuffer(Buffer::Desc
    {
        .size = inSize,
        .usage = Buffer::Usage::UPLOAD
    });

    Buffer& buffer = m_Device.GetBuffer(buffer_id);

    uint8_t* mapped_ptr = nullptr;
    const CD3DX12_RANGE range = CD3DX12_RANGE(0, 0);
    gThrowIfFailed(buffer->Map(0, &range, reinterpret_cast<void**>( &mapped_ptr )));

    memcpy(mapped_ptr, inData, inSize);

    assert(inBuffer.GetSize() >= inSize);
    inCmdList->CopyBufferRegion(inBuffer.GetD3D12Resource().Get(), inOffset, buffer.GetD3D12Resource().Get(), 0, inSize);

    m_Buffers.emplace_back(StagingBuffer
    {
        .mRetired    = false,
        .mFrameIndex = inCmdList.GetFrameIndex(),
        .mBufferID   = buffer_id,
        .mSize       = inSize,
        .mCapacity   = inSize,
        .mPtr        = mapped_ptr
    });
}



void StagingHeap::StageTexture(CommandList& inCmdList, const Texture& inTexture, uint32_t inSubResource, const void* inData)
{
    inCmdList.TrackResource(inTexture);

    uint32_t nr_of_rows = 0u;
    uint64_t row_size = 0ull, total_size = 0ull;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};

    auto desc = inTexture.GetD3D12Resource()->GetDesc();
    m_Device->GetCopyableFootprints(&desc, inSubResource, 1, 0, &footprint, &nr_of_rows, &row_size, &total_size);

    const uint64_t aligned_row_size = gAlignUp(row_size, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

    /*for (auto& buffer : m_Buffers) 
    {
        if (buffer.mRetired && inSize <= buffer.mCapacity - buffer.mSize) 
        {
            memcpy(buffer.mPtr + buffer.mSize, inData, aligned_size);

            const auto buffer_resource = m_Device.GetBuffer(buffer.mBufferID).GetD3D12Resource();
            const auto dest   = CD3DX12_TEXTURE_COPY_LOCATION(inResource.Get(), inSubResource);
            const auto source = CD3DX12_TEXTURE_COPY_LOCATION(buffer_resource.Get(), font_texture_footprint);

            inCmdList->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);

            buffer.mSize += aligned_size;
            buffer.mRetired = false;

            return;
        }
    }*/

    BufferID buffer_id = m_Device.CreateBuffer(Buffer::Desc {
        .size = uint32_t(total_size),
        .usage = Buffer::Usage::UPLOAD
    });


    Buffer& buffer = m_Device.GetBuffer(buffer_id);

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

    const auto dest = CD3DX12_TEXTURE_COPY_LOCATION(inTexture.GetD3D12Resource().Get(), inSubResource);
    const auto source = CD3DX12_TEXTURE_COPY_LOCATION(buffer.GetD3D12Resource().Get(), footprint);

    inCmdList->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);

    m_Buffers.emplace_back(StagingBuffer
    {
        .mRetired    = false,
        .mFrameIndex = inCmdList.GetFrameIndex(),
        .mBufferID   = buffer_id,
        .mSize       = total_size,
        .mCapacity   = total_size,
        .mPtr        = mapped_ptr,
    });

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(inTexture.GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ, inSubResource);
    inCmdList->ResourceBarrier(1, &barrier);
}



void StagingHeap::RetireBuffers(CommandList& inCmdList)
{
    for (auto& buffer : m_Buffers)
    {
        if (buffer.mFrameIndex == inCmdList.GetFrameIndex())
        {
            buffer.mSize = 0;
            buffer.mRetired = true;
        }
    }
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

    m_Buffer = BufferID::INVALID;
    m_Size = 0;
    m_DataPtr = nullptr;
    m_TotalCapacity = 0;
}



void GlobalConstantsAllocator::CreateBuffer(Device& inDevice)
{
    m_Buffer = inDevice.CreateBuffer(Buffer::Desc { .size = sizeof(GlobalConstants), .usage = Buffer::Usage::UPLOAD});

    auto buffer_range = CD3DX12_RANGE(0, 0);
    gThrowIfFailed(inDevice.GetBuffer(m_Buffer)->Map(0, &buffer_range, reinterpret_cast<void**>( &m_DataPtr )));
}



void GlobalConstantsAllocator::DestroyBuffer(Device& inDevice)
{
    if (!m_Buffer.IsValid())
        return;

    inDevice.GetBuffer(m_Buffer)->Unmap(0, nullptr);
    inDevice.ReleaseBuffer(m_Buffer);

    m_Buffer = BufferID::INVALID;
    m_DataPtr = nullptr;
}


} // namespace::Raekor
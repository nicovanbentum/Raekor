#include "pch.h"
#include "Device.h"

#include "Samplers.h"
#include "RenderUtil.h"
#include "CommandList.h"
#include "RenderGraph.h"

#include "OS.h"
#include "Maths.h"
#include "Timer.h"
#include "Threading.h"

#include <locale>
#include <codecvt>

namespace RK::DX12 {

Device::Device()
{
    uint32_t device_creation_flags = 0u;

#ifdef RK_DEBUG_BUILD
    ComPtr<ID3D12Debug1> debug_interface = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface))))
    {
        static bool debug_layer_enabled = OS::sCheckCommandLineOption("-debug_layer");
        static bool gpu_validation_enabled = OS::sCheckCommandLineOption("-gpu_validation");

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
        WString temp_path = fs::temp_directory_path().wstring();

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

    const D3D12MA::ALLOCATOR_DESC allocator_desc = { .pDevice = m_Device.Get(), .pAdapter = m_Adapter.Get() };
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

    m_ClearHeap.Allocate(*this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, sMaxClearHeapSize, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Allocate(*this, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, sMaxRTVHeapSize, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Allocate(*this, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, sMaxDSVHeapSize, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].Allocate(*this, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, sMaxSamplerHeapSize, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Allocate(*this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, sMaxResourceHeapSize, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

    for (uint32_t sampler_index = 0u; sampler_index < ESamplerIndex::SAMPLER_COUNT; sampler_index++)
        m_Device->CreateSampler(&SAMPLER_DESC[sampler_index], m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].GetCPUDescriptorHandle(DescriptorID(sampler_index)));

    uint32_t b_registers = 0u, t_registers = 0u;
    Array<D3D12_ROOT_PARAMETER1> root_params;

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
        constexpr std::array sizes = 
        { 
            uint32_t(sizeof(D3D12_DRAW_ARGUMENTS)), 
            uint32_t(sizeof(D3D12_DRAW_INDEXED_ARGUMENTS)), 
            uint32_t(sizeof(D3D12_DISPATCH_ARGUMENTS)) 
        };

        static_assert( sizes.size() == COMMAND_SIGNATURE_COUNT );

        D3D12_INDIRECT_ARGUMENT_DESC indirect_args = { .Type = (D3D12_INDIRECT_ARGUMENT_TYPE)cmd_sig };

        const D3D12_COMMAND_SIGNATURE_DESC cmdsig_desc =
        {
            .ByteStride = sizes[cmd_sig], .NumArgumentDescs = 1, .pArgumentDescs = &indirect_args
        };

        m_Device->CreateCommandSignature(&cmdsig_desc, nullptr, IID_PPV_ARGS(m_CommandSignatures[cmd_sig].GetAddressOf()));
    }
}


TextureID Device::CreateTexture(const Texture::Desc& inDesc, const Texture& inTexture)
{
    if (inDesc.debugName != nullptr)
        gSetDebugName(inTexture.m_Resource.Get(), inDesc.debugName);

    const TextureID texture_id = m_Textures.Add(inTexture);

    CreateDescriptor(texture_id, inDesc);

    return texture_id;
}


TextureID Device::CreateTexture(const Texture::Desc& inDesc)
{
    Texture texture = Texture(inDesc);
    D3D12_RESOURCE_DESC resource_desc = inDesc.ToResourceDesc();
    D3D12MA::ALLOCATION_DESC alloc_desc = inDesc.ToAllocationDesc();

    D3D12_CLEAR_VALUE clear_value = {};
    D3D12_CLEAR_VALUE* clear_value_ptr = nullptr;
    D3D12_RESOURCE_STATES initial_state = GetD3D12InitialResourceStates(inDesc.usage);

    if (inDesc.usage == Texture::DEPTH_STENCIL_TARGET)
    {
        clear_value = CD3DX12_CLEAR_VALUE(inDesc.format, 1.0f, 0.0f);
        clear_value_ptr = &clear_value;
    }
    else if (inDesc.usage == Texture::RENDER_TARGET)
    {
        float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        clear_value = CD3DX12_CLEAR_VALUE(inDesc.format, color);
        clear_value_ptr = &clear_value;
    }

    gThrowIfFailed(m_Allocator->CreateResource(&alloc_desc, &resource_desc, initial_state, clear_value_ptr, texture.m_Allocation.GetAddressOf(), IID_PPV_ARGS(&texture.m_Resource)));

    return CreateTexture(inDesc, texture);
}


BufferID Device::CreateBuffer(const Buffer::Desc& inDesc)
{
    Buffer buffer = Buffer(inDesc);
    D3D12_RESOURCE_DESC buffer_desc = inDesc.ToResourceDesc();
    D3D12MA::ALLOCATION_DESC alloc_desc = inDesc.ToAllocationDesc();

    D3D12_CLEAR_VALUE clear_value = {};
    D3D12_CLEAR_VALUE* clear_value_ptr = nullptr;
    D3D12_RESOURCE_STATES initial_state = GetD3D12InitialResourceStates(inDesc.usage);

    gThrowIfFailed(m_Allocator->CreateResource(&alloc_desc, &buffer_desc, initial_state, clear_value_ptr, buffer.m_Allocation.GetAddressOf(), IID_PPV_ARGS(&buffer.m_Resource)));

    return CreateBuffer(inDesc, buffer);
}


BufferID Device::CreateBuffer(const Buffer::Desc& inDesc, const Buffer& inBuffer)
{
    assert(inDesc.debugName);
    gSetDebugName(inBuffer.m_Resource.Get(), inDesc.debugName);

    const BufferID buffer_id = m_Buffers.Add(inBuffer);

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
    const Buffer& buffer = GetBuffer(inBufferID);

    assert(inBufferID.IsValid());
    m_Buffers.Remove(inBufferID);

    if (buffer.HasDescriptor())
        ReleaseDescriptor(buffer.GetUsage(), buffer.GetDescriptor());
}



void Device::ReleaseTexture(TextureID inTextureID)
{
    const Texture& texture = GetTexture(inTextureID);

    assert(inTextureID.IsValid());
    m_Textures.Remove(inTextureID);

    if (texture.HasDescriptor())
        ReleaseDescriptor(texture.GetUsage(), texture.GetDescriptor());
}



void Device::ReleaseBufferImmediate(BufferID inBufferID)
{
    assert(inBufferID.IsValid());

    Buffer& buffer = GetBuffer(inBufferID);
    buffer.m_Resource = nullptr;
    buffer.m_Allocation = nullptr;

    m_Buffers.Remove(inBufferID);

    if (buffer.HasDescriptor())
        ReleaseDescriptorImmediate(buffer.GetUsage(), buffer.GetDescriptor());
}



void Device::ReleaseTextureImmediate(TextureID inTextureID)
{
    assert(inTextureID.IsValid());

    Texture& texture = GetTexture(inTextureID);
    texture.m_Resource = nullptr;
    texture.m_Allocation = nullptr;

    m_Textures.Remove(inTextureID);

    if (texture.HasDescriptor())
        ReleaseDescriptorImmediate(texture.GetUsage(), texture.GetDescriptor());
}





D3D12_GRAPHICS_PIPELINE_STATE_DESC Device::CreatePipelineStateDesc(IRenderPass* inRenderPass, const ByteSlice& inVertexShader, const ByteSlice& inPixelShader)
{
    assert(inRenderPass->IsGraphics() && "Cannot create a Graphics PSO description for a Compute RenderPass");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state =
    {
        .pRootSignature = m_GlobalRootSignature.Get(),
        .VS = CD3DX12_SHADER_BYTECODE(inVertexShader.GetPtr(), inVertexShader.Length()),
        .PS = CD3DX12_SHADER_BYTECODE(inPixelShader.GetPtr(), inPixelShader.Length()),
        .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
        .SampleMask = UINT_MAX,
        .RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
        .DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
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



D3D12_COMPUTE_PIPELINE_STATE_DESC Device::CreatePipelineStateDesc(IRenderPass* inRenderPass, const ByteSlice& inComputeShader)
{
    assert(inRenderPass ? inRenderPass->IsCompute() : true && "Cannot create a Compute PSO description for a GraphicsRenderPass");
    D3D12_SHADER_BYTECODE cs_bytecode = CD3DX12_SHADER_BYTECODE(inComputeShader.GetPtr(), inComputeShader.Length());
    return D3D12_COMPUTE_PIPELINE_STATE_DESC { .pRootSignature = m_GlobalRootSignature.Get(), .CS = cs_bytecode };
}



void Device::CreateDescriptor(BufferID inID, const Buffer::Desc& inDesc)
{
    Buffer& buffer = GetBuffer(inID);
    buffer.m_Desc.usage = inDesc.usage;

    switch (inDesc.usage)
    {
        case Buffer::READBACK: break;
        case Buffer::SHADER_READ_WRITE:
        {
            const D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = inDesc.ToUAVDesc();
            buffer.m_Descriptor = CreateUnorderedAccessView(buffer.m_Resource, &uav_desc);
        } break;

        case Buffer::UPLOAD:
        case Buffer::GENERAL:
        case Buffer::INDEX_BUFFER:
        case Buffer::VERTEX_BUFFER:
        case Buffer::SHADER_READ_ONLY:
        case Buffer::INDIRECT_ARGUMENTS:
        case Buffer::ACCELERATION_STRUCTURE:
        {
            D3D12ResourceRef resource = buffer.GetD3D12Resource();
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = inDesc.ToSRVDesc();

            if (inDesc.usage == Buffer::ACCELERATION_STRUCTURE)
            {
                srv_desc.RaytracingAccelerationStructure.Location = buffer->GetGPUVirtualAddress();
                // resource must be NULL, since the resource location comes from a GPUVA in desc 
                buffer.m_Descriptor = CreateShaderResourceView(nullptr, &srv_desc);
            }
            else if (inDesc.format != DXGI_FORMAT_UNKNOWN || inDesc.stride > 0)
            {
                buffer.m_Descriptor = CreateShaderResourceView(resource, &srv_desc);
            }

        } break;

        default:
            assert(false); // should not be able to get here
    }
}



void Device::CreateDescriptor(TextureID inID, const Texture::Desc& inDesc)
{
    Texture& texture = GetTexture(inID);
    texture.m_Desc.usage = inDesc.usage;

    switch (inDesc.usage)
    {
        case Texture::DEPTH_STENCIL_TARGET:
        {
            const D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = inDesc.ToDSVDesc();
            texture.m_Descriptor = CreateDepthStencilView(texture.m_Resource, &dsv_desc);
        } break;

        case Texture::RENDER_TARGET:
        {
            const D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = inDesc.ToRTVDesc();
            texture.m_Descriptor = CreateRenderTargetView(texture.m_Resource, &rtv_desc);
        } break;

        case Texture::SHADER_READ_ONLY:
        {
            const D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = inDesc.ToSRVDesc();
            texture.m_Descriptor = CreateShaderResourceView(texture.m_Resource, &srv_desc);
        } break;

        case Texture::SHADER_READ_WRITE:
        {
            const D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = inDesc.ToUAVDesc();
            texture.m_Descriptor = CreateUnorderedAccessView(texture.m_Resource, &uav_desc);
        } break;

        default:
            assert(false); // should not be able to get here
    }
}


void Device::ReleaseDescriptor(Buffer::Usage inUsage, DescriptorID inDescriptorID)
{
    UNREFERENCED_PARAMETER(inUsage); // I just like API consistency
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Remove(inDescriptorID);
}


void Device::ReleaseDescriptor(Texture::Usage inUsage, DescriptorID inDescriptorID)
{
    D3D12_DESCRIPTOR_HEAP_TYPE heap_type;

    switch (inUsage)
    {
        case Texture::DEPTH_STENCIL_TARGET:
            heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV; break;
        case Texture::RENDER_TARGET:
            heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; break;
        case Texture::SHADER_READ_ONLY:
        case Texture::SHADER_READ_WRITE:
            heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; break;
        default:
            assert(false);
    }

    m_Heaps[heap_type].Remove(inDescriptorID);
}


void Device::ReleaseDescriptorImmediate(Buffer::Usage inUsage, DescriptorID inDescriptorID)
{
    UNREFERENCED_PARAMETER(inUsage); // I just like API consistency
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Get(inDescriptorID) = nullptr;
    m_Heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Remove(inDescriptorID);
}

void Device::ReleaseDescriptorImmediate(Texture::Usage inUsage, DescriptorID inDescriptorID)
{
    D3D12_DESCRIPTOR_HEAP_TYPE heap_type;

    switch (inUsage)
    {
        case Texture::DEPTH_STENCIL_TARGET: 
            heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV; break;
        case Texture::RENDER_TARGET:
            heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; break;
        case Texture::SHADER_READ_ONLY:
        case Texture::SHADER_READ_WRITE:
            heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; break;
        default:
            assert(false);
    }

    m_Heaps[heap_type].Get(inDescriptorID) = nullptr;
    m_Heaps[heap_type].Remove(inDescriptorID);
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
    assert(gIsDepthFormat(inResource->GetDesc().Format));

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



void Device::UploadBufferData(CommandList& inCmdList, const Buffer& inBuffer, uint32_t inOffset, const void* inData, uint32_t inSize)
{
    inCmdList.TrackResource(inBuffer);

    for (UploadBuffer& buffer : m_UploadBuffers)
    {
        if (buffer.mRetired && inSize <= buffer.mCapacity - buffer.mSize)
        {
            memcpy(buffer.mPtr + buffer.mSize, inData, inSize);

            ID3D12Resource* buffer_resource = GetD3D12Resource(buffer.mID);
            inCmdList->CopyBufferRegion(inBuffer.GetD3D12Resource().Get(), inOffset, buffer_resource, buffer.mSize, inSize);

            buffer.mSize += inSize;
            buffer.mRetired = false;
            buffer.mFrameIndex = inCmdList.GetFrameIndex();

            return;
        }
    }

    BufferID buffer_id = CreateBuffer(Buffer::Desc
    {
        .size = inSize,
        .usage = Buffer::Usage::UPLOAD,
        .debugName = "StagingBuffer"
    });

    Buffer& buffer = GetBuffer(buffer_id);

    uint8_t* mapped_ptr = nullptr;
    const CD3DX12_RANGE range = CD3DX12_RANGE(0, 0);
    gThrowIfFailed(buffer->Map(0, &range, reinterpret_cast<void**>( &mapped_ptr )));

    memcpy(mapped_ptr, inData, inSize);

    assert(inBuffer.GetSize() >= inSize);
    inCmdList->CopyBufferRegion(inBuffer.GetD3D12Resource().Get(), inOffset, buffer.GetD3D12Resource().Get(), 0, inSize);

    m_UploadBuffers.emplace_back(UploadBuffer
    {
        .mRetired    = false,
        .mFrameIndex = inCmdList.GetFrameIndex(),
        .mSize       = inSize,
        .mCapacity   = inSize,
        .mPtr        = mapped_ptr,
        .mID        = buffer_id,
    });
}



void Device::UploadTextureData(CommandList& inCmdList, const Texture& inTexture, uint32_t inSubResource, const void* inData)
{
    inCmdList.TrackResource(inTexture);

    uint32_t nr_of_rows = 0u;
    uint64_t row_size = 0ull, total_size = 0ull;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};

    D3D12_RESOURCE_DESC desc = inTexture.GetD3D12Resource()->GetDesc();
    m_Device->GetCopyableFootprints(&desc, inSubResource, 1, 0, &footprint, &nr_of_rows, &row_size, &total_size);

    for (UploadBuffer& buffer : m_UploadBuffers) 
    {
        if (buffer.mRetired && total_size <= buffer.mCapacity - buffer.mSize) 
        {
            uint8_t* data_ptr = (uint8_t*)inData;

            for (uint32_t row = 0u; row < nr_of_rows; row++)
            {
                uint8_t* copy_src = data_ptr + row * row_size;
                uint8_t* copy_dst = buffer.mPtr + row * footprint.Footprint.RowPitch;
                memcpy(copy_dst, copy_src, row_size);
            }

            CD3DX12_TEXTURE_COPY_LOCATION src(GetD3D12Resource(buffer.mID), footprint);
            CD3DX12_TEXTURE_COPY_LOCATION dst(inTexture.GetD3D12Resource().Get(), inSubResource);

            inCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

            buffer.mSize += total_size;
            buffer.mRetired = false;

            return;
        }
    }

    BufferID buffer_id = CreateBuffer(Buffer::Describe(total_size, Buffer::Usage::UPLOAD, false, "StagingBuffer"));
    Buffer& buffer = GetBuffer(buffer_id);

    uint8_t* mapped_ptr = nullptr;
    CD3DX12_RANGE range = CD3DX12_RANGE(0, 0);
    gThrowIfFailed(buffer->Map(0, &range, reinterpret_cast<void**>( &mapped_ptr )));

    mapped_ptr += footprint.Offset;
    uint8_t* data_ptr = (uint8_t*)inData;

    for (uint32_t row = 0u; row < nr_of_rows; row++)
    {
        uint8_t* copy_src = data_ptr + row * row_size;
        uint8_t* copy_dst = mapped_ptr + row * footprint.Footprint.RowPitch;
        memcpy(copy_dst, copy_src, row_size);
    }

    CD3DX12_TEXTURE_COPY_LOCATION src(buffer.GetD3D12Resource().Get(), footprint);
    CD3DX12_TEXTURE_COPY_LOCATION dst(inTexture.GetD3D12Resource().Get(), inSubResource);

    inCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    m_UploadBuffers.emplace_back(UploadBuffer
    {
        .mRetired    = false,
        .mFrameIndex = inCmdList.GetFrameIndex(),
        .mSize       = total_size,
        .mCapacity   = total_size,
        .mPtr        = mapped_ptr,
        .mID         = buffer_id
    });

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(inTexture.GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ, inSubResource);
    inCmdList->ResourceBarrier(1, &barrier);
}



void Device::RetireUploadBuffers(CommandList& inCmdList)
{
    for (UploadBuffer& buffer : m_UploadBuffers)
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

    m_Buffer = inDevice.CreateBuffer(Buffer::Desc {.size = inCapacity, .usage = Buffer::Usage::UPLOAD, .debugName = "RingAllocatorBuffer" });

    CD3DX12_RANGE buffer_range = CD3DX12_RANGE(0, 0);
    gThrowIfFailed(inDevice.GetBuffer(m_Buffer)->Map(0, &buffer_range, reinterpret_cast<void**>( &m_DataPtr )));
}



void RingAllocator::DestroyBuffer(Device& inDevice)
{
    if (!m_Buffer.IsValid())
        return;

    inDevice.GetBuffer(m_Buffer)->Unmap(0, nullptr);
    inDevice.ReleaseBuffer(m_Buffer);

    m_Buffer = BufferID();
    m_Size = 0;
    m_DataPtr = nullptr;
    m_TotalCapacity = 0;
}

/*
Allocates memory and memcpy's inData to the mapped buffer. ioOffset contains the offset from the starting pointer.
This function default aligns to 4, so the offset can be used with HLSL byte address buffers directly:
ByteAddressBuffer buffer;
T data = buffer.Load<T>(ioOffset);
*/

 void RingAllocator::AllocAndCopy(uint32_t inSize, const void* inData, uint32_t& ioOffset, uint32_t inAlignment)
{
    const auto size = gAlignUp(inSize, inAlignment);
    //assert(m_Size + size <= m_TotalCapacity);
    m_Size += size;

    if (m_Size >= m_TotalCapacity)
        m_Size = 0;

    memcpy(m_DataPtr + m_Size, inData, inSize);
    ioOffset = m_Size;
}



void GlobalConstantsAllocator::CreateBuffer(Device& inDevice)
{
    m_Buffer = inDevice.CreateBuffer(Buffer::Desc 
    { 
        .size  = sizeof(GlobalConstants), 
        .usage = Buffer::Usage::UPLOAD, 
        .debugName = "GlobalConstantsBuffer"
    });

    CD3DX12_RANGE buffer_range = CD3DX12_RANGE(0, 0);
    gThrowIfFailed(inDevice.GetBuffer(m_Buffer)->Map(0, &buffer_range, reinterpret_cast<void**>( &m_DataPtr )));
}



void GlobalConstantsAllocator::DestroyBuffer(Device& inDevice)
{
    if (!m_Buffer.IsValid())
        return;

    inDevice.GetBuffer(m_Buffer)->Unmap(0, nullptr);
    inDevice.ReleaseBuffer(m_Buffer);

    m_Buffer = BufferID();
    m_DataPtr = nullptr;
}


} // namespace::Raekor
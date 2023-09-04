#include "pch.h"
#include "DXRenderer.h"

#include "shared.h"
#include "DXScene.h"
#include "DXShader.h"
#include "DXRenderGraph.h"

#include "Raekor/OS.h"
#include "Raekor/gui.h"
#include "Raekor/timer.h"
#include "Raekor/primitives.h"
#include "Raekor/application.h"

namespace Raekor::DX12 {

Renderer::Renderer(Device& inDevice, const Viewport& inViewport, SDL_Window* inWindow) : 
    m_Window(inWindow),
    m_RenderGraph(inDevice, inViewport, sFrameCount) 
{
    auto swapchain_desc = DXGI_SWAP_CHAIN_DESC1 {
        .Width       = inViewport.size.x,
        .Height      = inViewport.size.y,
        .Format      = sSwapchainFormat,
        .SampleDesc  = DXGI_SAMPLE_DESC {.Count = 1 },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = sFrameCount,
        .SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD,
    };

    if (inDevice.IsTearingSupported())
        swapchain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    auto factory = ComPtr<IDXGIFactory4>{};
    gThrowIfFailed(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory)));

    auto wmInfo = SDL_SysWMinfo{};
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(inWindow, &wmInfo);
    auto hwnd = wmInfo.info.win.window;

    auto swapchain = ComPtr<IDXGISwapChain1>{};
    gThrowIfFailed(factory->CreateSwapChainForHwnd(inDevice.GetQueue(), hwnd, &swapchain_desc, nullptr, nullptr, &swapchain));
    gThrowIfFailed(swapchain.As(&m_Swapchain));

    // Disables DXGI's automatic ALT+ENTER and PRINTSCREEN
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_WINDOW_CHANGES);

    SDL_DisplayMode mode;
    SDL_GetDisplayMode(SDL_GetWindowDisplayIndex(m_Window), m_Settings.mDisplayRes, &mode);
    SDL_SetWindowDisplayMode(m_Window, &mode);

    m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

    for (const auto& [index, backbuffer_data] : gEnumerate(m_BackBufferData)) {
        auto rtv_resource = ResourceRef{};
        gThrowIfFailed(m_Swapchain->GetBuffer(index, IID_PPV_ARGS(rtv_resource.GetAddressOf())));
        backbuffer_data.mBackBuffer = inDevice.CreateTextureView(rtv_resource, Texture::Desc{ .usage = Texture::Usage::RENDER_TARGET });

        backbuffer_data.mDirectCmdList = CommandList(inDevice);
        backbuffer_data.mDirectCmdList->SetName(L"Raekor::DX12::CommandList");

        rtv_resource->SetName(L"BACKBUFFER");
    }

    inDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));
    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (!m_FenceEvent)
        gThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));

    auto fsr2_desc = FfxFsr2ContextDescription {
        .flags          = FfxFsr2InitializationFlagBits::FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE,
        .maxRenderSize  = { inViewport.size.x, inViewport.size.y },
        .displaySize    = { inViewport.size.x, inViewport.size.y },
        .device         = ffxGetDeviceDX12(inDevice),
    };

    m_FsrScratchMemory.resize(ffxFsr2GetScratchMemorySizeDX12());
    auto ffx_error = ffxFsr2GetInterfaceDX12(&fsr2_desc.callbacks, inDevice, m_FsrScratchMemory.data(), m_FsrScratchMemory.size());
    assert(ffx_error == FFX_OK);

    ffx_error = ffxFsr2ContextCreate(&m_Fsr2Context, &fsr2_desc);
    assert(ffx_error == FFX_OK);
}



void Renderer::OnResize(Device& inDevice, const Viewport& inViewport, bool inFullScreen) {
    for (auto& bb_data : m_BackBufferData)
        inDevice.ReleaseTextureImmediate(bb_data.mBackBuffer);

    auto desc = DXGI_SWAP_CHAIN_DESC {};
    gThrowIfFailed(m_Swapchain->GetDesc(&desc));


    auto window_width = 0, window_height = 0;
    SDL_GetWindowSize(m_Window, &window_width, &window_height);
    gThrowIfFailed(m_Swapchain->ResizeBuffers(desc.BufferCount, window_width, window_height, desc.BufferDesc.Format, desc.Flags));

    for (const auto& [index, backbuffer_data] : gEnumerate(m_BackBufferData)) {
        auto rtv_resource = ResourceRef(nullptr);
        gThrowIfFailed(m_Swapchain->GetBuffer(index, IID_PPV_ARGS(rtv_resource.GetAddressOf())));

        backbuffer_data.mBackBuffer = inDevice.CreateTextureView(rtv_resource, Texture::Desc{ .usage = Texture::Usage::RENDER_TARGET });
        rtv_resource->SetName(L"BACKBUFFER");
    }

    m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

    ffxFsr2ContextDestroy(&m_Fsr2Context);

    auto fsr2_desc = FfxFsr2ContextDescription {
        .flags          = FfxFsr2InitializationFlagBits::FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE,
        .maxRenderSize  = { inViewport.size.x, inViewport.size.y },
        .displaySize    = { inViewport.size.x, inViewport.size.y },
        .device         = ffxGetDeviceDX12(inDevice),
    };

    m_FsrScratchMemory.resize(ffxFsr2GetScratchMemorySizeDX12());
    auto ffx_error = ffxFsr2GetInterfaceDX12(&fsr2_desc.callbacks, inDevice, m_FsrScratchMemory.data(), m_FsrScratchMemory.size());
    assert(ffx_error == FFX_OK);

    ffx_error = ffxFsr2ContextCreate(&m_Fsr2Context, &fsr2_desc);
    assert(ffx_error == FFX_OK);

    m_Settings.mFullscreen = inFullScreen;
    std::cout << std::format("Render Size: {}, {} \n", inViewport.size.x, inViewport.size.y);
}



void Renderer::OnRender(Application* inApp, Device& inDevice, const Viewport& inViewport, RayTracedScene& inScene, StagingHeap& inStagingHeap, IRenderInterface* inRenderInterface, float inDeltaTime) {
    // Check if any of the shader sources were updated and recompile them if necessary
    bool need_recompile = g_SystemShaders.HotLoad();
    if (need_recompile)
        std::cout << std::format("Hotloaded system shaders.\n");

    if (m_ShouldResize || need_recompile) {
        // Make sure nothing is using render targets anymore
        WaitForIdle(inDevice);

        // Resize the renderer, which recreates the swapchain backbuffers and re-inits FSR2
        OnResize(inDevice, inViewport, SDL_IsWindowExclusiveFullscreen(m_Window));

        // Recompile the renderer, probably a bit overkill. TODO: pls fix
        Recompile(inDevice, inScene, inRenderInterface);

        // Unflag
        m_ShouldResize = false;

        return;
    }

    if (m_FrameCounter > 0) {
        // At this point in the frame we really need to previous frame's present job to have finished
        if (m_PresentJobPtr)
            m_PresentJobPtr->WaitCPU();

        auto& backbuffer_data = GetBackBufferData();
        auto  completed_value = m_Fence->GetCompletedValue();

        // make sure the backbuffer data we're about to use is no longer being used by the GPU
        if (completed_value < backbuffer_data.mFenceValue) {
            gThrowIfFailed(m_Fence->SetEventOnCompletion(backbuffer_data.mFenceValue, m_FenceEvent));
            WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
        }
    }

    // Update the total running time of the application / renderer
    m_ElapsedTime += inDeltaTime;

    // Calculate a jittered projection matrix for FSR2
    const auto& camera = m_RenderGraph.GetViewport().GetCamera();
    const auto jitter_phase_count = ffxFsr2GetJitterPhaseCount(m_RenderGraph.GetViewport().size.x, m_RenderGraph.GetViewport().size.y);

    float jitter_offset_x = 0;
    float jitter_offset_y = 0;
    ffxFsr2GetJitterOffset(&jitter_offset_x, &jitter_offset_y, m_FrameCounter, jitter_phase_count);

    const auto jitter_x = 2.0f * jitter_offset_x / (float)m_RenderGraph.GetViewport().size.x;
    const auto jitter_y = -2.0f * jitter_offset_y / (float)m_RenderGraph.GetViewport().size.y;
    const auto offset_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(jitter_x, jitter_y, 0));
    const auto jittered_proj_matrix = camera.GetProjection();
    //const auto jittered_proj_matrix = offset_matrix * m_Camera.GetProjection();

    // Update all the frame constants and copy it in into the GPU ring buffer
    m_FrameConstants.mTime                       = m_ElapsedTime;
    m_FrameConstants.mDeltaTime                  = inDeltaTime;
    m_FrameConstants.mFrameIndex                 = m_FrameCounter % sFrameCount;
    m_FrameConstants.mFrameCounter               = m_FrameCounter;
    m_FrameConstants.mSunColor                   = inScene->GetSunLight() ? inScene->GetSunLight()->GetColor() : Vec4(1.0f);
    m_FrameConstants.mSunDirection               = Vec4(inScene->GetSunLightDirection(), 0.0f);
    m_FrameConstants.mCameraPosition             = Vec4(camera.GetPosition(), 1.0f);
    m_FrameConstants.mViewMatrix                 = camera.GetView();
    m_FrameConstants.mProjectionMatrix           = jittered_proj_matrix;
    m_FrameConstants.mPrevViewProjectionMatrix   = m_FrameConstants.mViewProjectionMatrix;
    m_FrameConstants.mViewProjectionMatrix       = jittered_proj_matrix * camera.GetView();
    m_FrameConstants.mInvViewProjectionMatrix    = glm::inverse(jittered_proj_matrix * camera.GetView());

    m_RenderGraph.GetPerFrameAllocator().AllocAndCopy(m_FrameConstants, m_RenderGraph.GetPerFrameAllocatorOffset());

    // Update any per pass data here, before executing the render graph
    if (auto fsr_pass = m_RenderGraph.GetPass<FSR2Data>())
        fsr_pass->GetData().mDeltaTime = inDeltaTime;

    if (m_ShouldCaptureNextFrame) {
        PIXCaptureParameters capture_params = {};
        capture_params.GpuCaptureParameters.FileName = L"temp_pix_capture.wpix";
        gThrowIfFailed(PIXBeginCapture(PIX_CAPTURE_GPU, &capture_params));
    }

    // Start recording copy commands
    //auto& copy_cmd_list = GetBackBufferData().mCopyCmdList;
    //copy_cmd_list.Reset();

    //// Record copy commands to the copy cmd list
    //inScene.UploadTLAS(inApp, inDevice, inStagingHeap, copy_cmd_list);

    //// Submit all copy commands
    //copy_cmd_list.Close();
    //copy_cmd_list.Submit(inDevice);

    // Start recording graphics commands
    auto& direct_cmd_list = GetBackBufferData().mDirectCmdList;
    direct_cmd_list.Reset();

    // Set the back buffer
    // Its kind of annoying that we have to set this every frame instead of once, TODO: pls fix
    m_RenderGraph.SetBackBuffer(GetBackBufferData().mBackBuffer);

    // Record the entire frame into the direct cmd list
    m_RenderGraph.Execute(inDevice, direct_cmd_list, m_FrameCounter);

    //Record commands to render ImGui to the backbuffer
    RenderImGui(m_RenderGraph, inDevice, direct_cmd_list);

    direct_cmd_list.Close();

    // Run command list execution and present in a job so it can overlap a bit with the start of the next frame
    //m_PresentJobPtr = Async::sQueueJob([&inDevice, this]() {
        // submit all graphics commands 
        direct_cmd_list.Submit(inDevice);

        auto sync_interval = m_Settings.mEnableVsync;
        auto present_flags = 0u;

        if (!m_Settings.mEnableVsync && inDevice.IsTearingSupported())
            present_flags = DXGI_PRESENT_ALLOW_TEARING;

        gThrowIfFailed(m_Swapchain->Present(sync_interval, present_flags));
     
        GetBackBufferData().mFenceValue++;
        m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();
        gThrowIfFailed(inDevice.GetQueue()->Signal(m_Fence.Get(), GetBackBufferData().mFenceValue));
    //});

    m_FrameCounter++;

    if (m_ShouldCaptureNextFrame) {
        // Wait for the present job here to make sure we capture everything
        if (m_PresentJobPtr)
            m_PresentJobPtr->WaitCPU();

        gThrowIfFailed(PIXEndCapture(FALSE));
        m_ShouldCaptureNextFrame = false;
        ShellExecute(0, 0, "temp_pix_capture.wpix", 0, 0, SW_SHOW);
    }
}



void Renderer::Recompile(Device& inDevice, const RayTracedScene& inScene, IRenderInterface* inRenderInterface) {
    m_RenderGraph.Clear(inDevice);
    m_RenderGraph.SetBackBuffer(GetBackBufferData().mBackBuffer);

    const auto& gbuffer_data = AddGBufferPass(m_RenderGraph, inDevice, inScene);
    
    auto compose_input = TextureResource{};

    if (m_Settings.mDoPathTrace) {
        compose_input = AddPathTracePass(m_RenderGraph, inDevice, inScene, gbuffer_data).mOutputTexture;
    } 
    else {
        // const auto& grass_data = AddGrassRenderPass(m_RenderGraph, inDevice, gbuffer_data);
    
        const auto& shadow_data = AddShadowMaskPass(m_RenderGraph, inDevice, inScene, gbuffer_data);

        const auto& rtao_data = AddAmbientOcclusionPass(m_RenderGraph, inDevice, inScene, gbuffer_data);

        const auto& reflection_data = AddReflectionsPass(m_RenderGraph, inDevice, inScene, gbuffer_data);

        const auto& downsample_data = AddDownsamplePass(m_RenderGraph, inDevice, reflection_data.mOutputTexture);
    
        const auto& ddgi_trace_data = AddProbeTracePass(m_RenderGraph, inDevice, inScene);

        const auto& ddgi_update_data = AddProbeUpdatePass(m_RenderGraph, inDevice, ddgi_trace_data);
    
        const auto& light_data = AddLightingPass(m_RenderGraph, inDevice, gbuffer_data, shadow_data, reflection_data, rtao_data, ddgi_update_data);
    
        if (m_Settings.mProbeDebug)
            const auto& probe_debug_data = AddProbeDebugPass(m_RenderGraph, inDevice, ddgi_trace_data, ddgi_update_data, light_data.mOutputTexture, gbuffer_data.mDepthTexture);

        if (m_Settings.mDebugLines) {
            const auto& debug_lines_data = AddDebugLinesPass(m_RenderGraph, inDevice, light_data.mOutputTexture, gbuffer_data.mDepthTexture);
            m_FrameConstants.mDebugLinesVertexBuffer = debug_lines_data.mVertexBuffer.GetBindlessIndex(inDevice);
            m_FrameConstants.mDebugLinesIndirectArgsBuffer = debug_lines_data.mIndirectArgsBuffer.GetBindlessIndex(inDevice);
        }

        compose_input = light_data.mOutputTexture;
    
        if (m_Settings.mEnableFsr2)
            compose_input = AddFsrPass(m_RenderGraph, inDevice, m_Fsr2Context, light_data.mOutputTexture, gbuffer_data).mOutputTexture;
    }

    const auto& compose_data = AddComposePass(m_RenderGraph, inDevice, compose_input);

    auto final_output = compose_data.mOutputTexture;

    const auto debug_texture = EDebugTexture(inRenderInterface->GetSettings().mDebugTexture);

    switch (debug_texture) {
        case DEBUG_TEXTURE_NONE:
            break;
        case DEBUG_TEXTURE_GBUFFER_DEPTH:
        case DEBUG_TEXTURE_GBUFFER_ALBEDO:
        case DEBUG_TEXTURE_GBUFFER_NORMALS:
        case DEBUG_TEXTURE_GBUFFER_METALLIC:
        case DEBUG_TEXTURE_GBUFFER_ROUGHNESS:
            final_output = AddGBufferDebugPass(m_RenderGraph, inDevice, gbuffer_data, debug_texture).mOutputTexture;
            break;
        case DEBUG_TEXTURE_GBUFFER_VELOCITY:
            final_output = gbuffer_data.mMotionVectorTexture;
            break;
        case DEBUG_TEXTURE_RT_SHADOWS:
            if (auto data = m_RenderGraph.GetPass<RTShadowMaskData>())
                final_output = data->GetData().mOutputTexture;
            break;
        case DEBUG_TEXTURE_RT_AO:
            if (auto data = m_RenderGraph.GetPass<RTAOData>())
                final_output = data->GetData().mOutputTexture;
            break;
        case DEBUG_TEXTURE_RT_REFLECTIONS:
            if (auto data = m_RenderGraph.GetPass<ReflectionsData>())
                final_output = data->GetData().mOutputTexture;
            break;
        case DEBUG_TEXTURE_LIGHTING:
            if (auto data = m_RenderGraph.GetPass<LightingData>())
                final_output = data->GetData().mOutputTexture;
            break;
        default:
            assert(false);
    }

    const auto& pre_imgui_data = AddPreImGuiPass(m_RenderGraph, inDevice, final_output);

    // const auto& imgui_data = AddImGuiPass(m_RenderGraph, inDevice, inStagingHeap, compose_data.mOutputTexture);

    m_RenderGraph.Compile(inDevice);
}



void Renderer::WaitForIdle(Device& inDevice) {
    if (m_PresentJobPtr)
        m_PresentJobPtr->WaitCPU();

    for (auto& backbuffer_data : m_BackBufferData) {
        gThrowIfFailed(inDevice.GetQueue()->Signal(m_Fence.Get(), backbuffer_data.mFenceValue));

        if (m_Fence->GetCompletedValue() < backbuffer_data.mFenceValue) {
            gThrowIfFailed(m_Fence->SetEventOnCompletion(backbuffer_data.mFenceValue, m_FenceEvent));
            WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
        }

        backbuffer_data.mFenceValue = 0;
    }

    gThrowIfFailed(m_Fence->Signal(0));
    m_FrameCounter = 0;
}



RenderInterface::RenderInterface(Device& inDevice, Renderer& inRenderer, StagingHeap& inStagingHeap) :
    IRenderInterface(GraphicsAPI::DirectX12), m_Device(inDevice), m_Renderer(inRenderer), m_StagingHeap(inStagingHeap)
{
    DXGI_ADAPTER_DESC adapter_desc = {};
    m_Device.GetAdapter()->GetDesc(&adapter_desc);

    GPUInfo gpu_info = {};
    switch (adapter_desc.VendorId) {
        case 0x000010de: gpu_info.mVendor = "NVIDIA Corporation";           break;
        case 0x00001002: gpu_info.mVendor = "Advanced Micro Devices, Inc."; break;
        case 0x00008086: gpu_info.mVendor = "Intel Corporation";            break;
        case 0x00001414: gpu_info.mVendor = "Microsoft Corporation";        break;
    }

    gpu_info.mProduct = gWCharToString(adapter_desc.Description);
    gpu_info.mActiveAPI = "DirectX 12 Ultimate";

    SetGPUInfo(gpu_info);
}



uint64_t RenderInterface::GetDisplayTexture() {
    if (auto pre_imgui_pass = m_Renderer.GetRenderGraph().GetPass<PreImGuiData>())
        return m_Device.GetGPUDescriptorHandle(pre_imgui_pass->GetData().mDisplayTexture.mResourceTexture).ptr;
    else
        return uint64_t(TextureID::INVALID);
}



uint64_t RenderInterface::GetImGuiTextureID(uint32_t inHandle)
{
    const auto& resource_heap = m_Device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetHeap();
    const auto heap_ptr = resource_heap->GetGPUDescriptorHandleForHeapStart().ptr + inHandle * m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    return heap_ptr;
}



const char* RenderInterface::GetDebugTextureName(uint32_t inIndex) const {
    constexpr auto names = std::array {
        "Final",
        "Depth",
        "Albedo",
        "Normals",
        "Velocity",
        "Metallic",
        "Roughness",
        "RT AO",
        "RT Shadows",
        "RT Reflections",
        "Lighting"
    };

    static_assert(names.size() == DEBUG_TEXTURE_COUNT);

    assert(inIndex < DEBUG_TEXTURE_COUNT);
    return names[inIndex];
}



void RenderInterface::UploadMeshBuffers(Mesh& inMesh) { 
    m_Renderer.WaitForIdle(m_Device);

    const auto vertices = inMesh.GetInterleavedVertices();
    const auto vertices_size = vertices.size() * sizeof(vertices[0]);
    const auto indices_size = inMesh.indices.size() * sizeof(inMesh.indices[0]);

    if (!vertices_size || !indices_size)
        return;

    inMesh.indexBuffer = m_Device.CreateBuffer(Buffer::Desc{
        .size   = uint32_t(indices_size),
        .stride = sizeof(uint32_t) * 3,
        .usage  = Buffer::Usage::INDEX_BUFFER
    }, L"INDEX_BUFFER").ToIndex();

    inMesh.vertexBuffer = m_Device.CreateBuffer(Buffer::Desc{
        .size   = uint32_t(vertices_size),
        .stride = sizeof(Vertex),
        .usage  = Buffer::Usage::VERTEX_BUFFER
    }, L"VERTEX_BUFFER").ToIndex();

    D3D12_RAYTRACING_GEOMETRY_DESC geom = {};
    geom.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geom.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    geom.Triangles.IndexBuffer = m_Device.GetBuffer(BufferID(inMesh.indexBuffer))->GetGPUVirtualAddress();
    geom.Triangles.IndexCount = inMesh.indices.size();
    geom.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

    geom.Triangles.VertexBuffer.StartAddress = m_Device.GetBuffer(BufferID(inMesh.vertexBuffer))->GetGPUVirtualAddress();
    geom.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
    geom.Triangles.VertexCount = inMesh.positions.size();
    geom.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.pGeometryDescs = &geom;
    inputs.NumDescs = 1;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info = {};
    m_Device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild_info);

    const auto blas_buffer_id = m_Device.CreateBuffer(Buffer::Desc {
        .size = prebuild_info.ResultDataMaxSizeInBytes,
        .usage = Buffer::Usage::ACCELERATION_STRUCTURE
    }, L"BLAS_BUFFER");

    inMesh.BottomLevelAS = blas_buffer_id.ToIndex();

    const auto scratch_buffer_id = m_Device.CreateBuffer(Buffer::Desc{
        .size = prebuild_info.ScratchDataSizeInBytes
    }, L"SCRATCH_BUFFER_BLAS_RT");

    auto& blas_buffer = m_Device.GetBuffer(blas_buffer_id);
    auto& scratch_buffer = m_Device.GetBuffer(scratch_buffer_id);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
    desc.ScratchAccelerationStructureData = scratch_buffer->GetGPUVirtualAddress();
    desc.DestAccelerationStructureData = blas_buffer->GetGPUVirtualAddress();
    desc.Inputs = inputs;

    auto& cmd_list = m_Renderer.StartSingleSubmit();

    {
        const auto& gpu_index_buffer  = m_Device.GetBuffer(BufferID(inMesh.indexBuffer));
        const auto& gpu_vertex_buffer = m_Device.GetBuffer(BufferID(inMesh.vertexBuffer));

        m_StagingHeap.StageBuffer(cmd_list, gpu_index_buffer.GetResource(), 0, inMesh.indices.data(), indices_size);
        m_StagingHeap.StageBuffer(cmd_list, gpu_vertex_buffer.GetResource(), 0, vertices.data(), vertices_size);
    }

    {
        const auto& gpu_index_buffer  = m_Device.GetBuffer(BufferID(inMesh.indexBuffer));
        const auto& gpu_vertex_buffer = m_Device.GetBuffer(BufferID(inMesh.vertexBuffer));

        const auto barriers = std::array {
            CD3DX12_RESOURCE_BARRIER::Transition(gpu_index_buffer.GetResource().Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ),
            CD3DX12_RESOURCE_BARRIER::Transition(gpu_vertex_buffer.GetResource().Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ)
        };

        cmd_list->ResourceBarrier(barriers.size(), barriers.data());
    }

    cmd_list->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);

    m_Renderer.FlushSingleSubmit(m_Device, cmd_list);
    
    // m_Device.ReleaseBuffer(scratch_buffer_id);
}


uint32_t RenderInterface::UploadTextureFromAsset(const TextureAsset::Ptr& inAsset, bool inIsSRGB) { 
    auto data_ptr = inAsset->GetData();
    const auto header_ptr = inAsset->GetHeader();
    // I think DirectStorage doesnt do proper texture data alignment for its upload buffers as we get garbage past the 4th mip
    ///const auto mipmap_levels = std::min(header_ptr->dwMipMapCount, 4ul);
    const auto mipmap_levels = header_ptr->dwMipMapCount;

    auto texture = m_Device.GetTexture(m_Device.CreateTexture(Texture::Desc{
        .format     = inIsSRGB ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM,
        .width      = header_ptr->dwWidth,
        .height     = header_ptr->dwHeight,
        .mipLevels  = mipmap_levels,
        .usage      = Texture::SHADER_READ_ONLY
    }, inAsset->GetPath().wstring().c_str()));

    auto& cmd_list = m_Renderer.StartSingleSubmit();

    for (uint32_t mip = 0; mip < mipmap_levels; mip++) {
        const auto dimensions = glm::ivec2(std::max(header_ptr->dwWidth >> mip, 1ul), std::max(header_ptr->dwHeight >> mip, 1ul));

        m_StagingHeap.StageTexture(cmd_list, texture.GetResource(), mip, data_ptr);

        data_ptr += dimensions.x * dimensions.y;
    }

    m_Renderer.FlushSingleSubmit(m_Device, cmd_list);

    return texture.GetView().ToIndex();
}



void RenderInterface::OnResize(const Viewport& inViewport) { 
    m_Renderer.SetShouldResize(true); 
}

CommandList& Renderer::StartSingleSubmit() {
    auto& cmd_list = GetBackBufferData().mDirectCmdList;
    cmd_list.Begin();
    return cmd_list;
}



void Renderer::FlushSingleSubmit(Device& inDevice, CommandList& inCmdList) {
    inCmdList.Close();

    const auto cmd_lists = std::array { (ID3D12CommandList*)inCmdList };
    inDevice.GetQueue()->ExecuteCommandLists(cmd_lists.size(), cmd_lists.data());

    auto& backbuffer_data = GetBackBufferData();
    backbuffer_data.mFenceValue++;
    gThrowIfFailed(inDevice.GetQueue()->Signal(m_Fence.Get(), backbuffer_data.mFenceValue));
    gThrowIfFailed(m_Fence->SetEventOnCompletion(backbuffer_data.mFenceValue, m_FenceEvent));
    WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


RTTI_DEFINE_TYPE(GBufferData)	      {}
RTTI_DEFINE_TYPE(GBufferDebugData)    {}
RTTI_DEFINE_TYPE(GrassData)           {}
RTTI_DEFINE_TYPE(RTShadowMaskData)    {}
RTTI_DEFINE_TYPE(RTAOData)            {}
RTTI_DEFINE_TYPE(ReflectionsData)     {}
RTTI_DEFINE_TYPE(DownsampleData)      {}
RTTI_DEFINE_TYPE(LightingData)        {}
RTTI_DEFINE_TYPE(FSR2Data)            {}
RTTI_DEFINE_TYPE(ProbeTraceData)      {}
RTTI_DEFINE_TYPE(ProbeUpdateData)     {}
RTTI_DEFINE_TYPE(PathTraceData)       {}
RTTI_DEFINE_TYPE(ProbeDebugData)      {}
RTTI_DEFINE_TYPE(DebugLinesData)      {}
RTTI_DEFINE_TYPE(ComposeData)         {}
RTTI_DEFINE_TYPE(PreImGuiData)        {}
RTTI_DEFINE_TYPE(ImGuiData)           {}

/*

const T& AddPass(RenderGraph& inRenderGraph, Device& inDevice) {
    return inRenderGraph.AddGraphicsPass<T>(pass_name, inDevice,
    [&](IRenderPass* inRenderPass, T& inData) {  },
    [](T& inData, CommandList& inCmdList) {   });
}  

*/


const GBufferData& AddGBufferPass(RenderGraph& inRenderGraph, Device& inDevice, const Scene& inScene) {
    return inRenderGraph.AddGraphicsPass<GBufferData>("GBUFFER PASS", inDevice,
    [&](IRenderPass* inRenderPass, GBufferData& inData)
    {
        const auto render_texture = inDevice.CreateTexture(Texture::Desc{
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET,
        }, L"GBUFFER_MATERIAL");

        const auto movec_texture = inDevice.CreateTexture(Texture::Desc{
            .format = DXGI_FORMAT_R32G32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET,
        }, L"GBUFFER_MOTION_VECTOR");

        const auto depth_texture = inDevice.CreateTexture(Texture::Desc{
            .format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::DEPTH_STENCIL_TARGET,
        }, L"GBUFFER_DEPTH");

        inRenderPass->Create(render_texture);
        inRenderPass->Create(movec_texture);
        inRenderPass->Create(depth_texture);

        inData.mDepthTexture        = inRenderPass->Write(depth_texture);
        inData.mRenderTexture       = inRenderPass->Write(render_texture); // SV_TARGET0
        inData.mMotionVectorTexture = inRenderPass->Write(movec_texture);  // SV_TARGET1

        CD3DX12_SHADER_BYTECODE vertex_shader, pixel_shader;
        g_SystemShaders.mGBufferShader.GetGraphicsProgram(vertex_shader, pixel_shader);
        
        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, vertex_shader, pixel_shader);
        
        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(inData.mPipeline.GetAddressOf()));
        inData.mPipeline->SetName(L"PSO_GBUFFER");
    },

    [&inRenderGraph, &inDevice, &inScene](GBufferData& inData, CommandList& inCmdList)
    {
        const auto& viewport = inRenderGraph.GetViewport();
        inCmdList.SetViewportScissorRect(viewport);
        inCmdList->SetPipelineState(inData.mPipeline.Get());

        const auto clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        const auto clear_rect = CD3DX12_RECT(0, 0, viewport.size.x, viewport.size.y);
        inCmdList->ClearRenderTargetView(inDevice.GetHeapPtr(inData.mRenderTexture), glm::value_ptr(clear_color), 1, &clear_rect);
        inCmdList->ClearRenderTargetView(inDevice.GetHeapPtr(inData.mMotionVectorTexture), glm::value_ptr(clear_color), 1, &clear_rect);
        inCmdList->ClearDepthStencilView(inDevice.GetHeapPtr(inData.mDepthTexture), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 1, &clear_rect);

        for (const auto& [entity, transform, mesh] : inScene.Each<Transform, Mesh>()) {
            const auto& index_buffer = inDevice.GetBuffer(BufferID(mesh.indexBuffer));
            const auto& vertex_buffer = inDevice.GetBuffer(BufferID(mesh.vertexBuffer));

            const auto index_view = D3D12_INDEX_BUFFER_VIEW {
                .BufferLocation = index_buffer->GetGPUVirtualAddress(),
                .SizeInBytes    = uint32_t(mesh.indices.size() * sizeof(mesh.indices[0])),
                .Format         = DXGI_FORMAT_R32_UINT,
            };

            if (mesh.material == NULL_ENTITY)
                continue;

            auto material = inScene.GetPtr<Material>(mesh.material);
            if (material == nullptr)
                material = &Material::Default;

            auto root_constants = GbufferRootConstants {
                .mVertexBuffer       = vertex_buffer.GetHeapIndex(),
                .mAlbedoTexture      = material->gpuAlbedoMap,
                .mNormalTexture      = material->gpuNormalMap,
                .mMetalRoughTexture  = material->gpuMetallicRoughnessMap,
                .mAlbedo             = material->albedo,
                .mRoughness          = material->roughness,
                .mMetallic           = material->metallic,
                .mWorldTransform     = transform.worldTransform,
                .mInvWorldTransform  = glm::inverse(transform.worldTransform)
            };

            inCmdList->SetGraphicsRoot32BitConstants(0, sizeof(root_constants) / sizeof(DWORD), &root_constants, 0);

            inCmdList->IASetIndexBuffer(&index_view);

            if (entity == inData.mActiveEntity) {
                
            }

            inCmdList->DrawIndexedInstanced(mesh.indices.size(), 1, 0, 0, 0);
        }
    });
}



const GBufferDebugData& AddGBufferDebugPass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferData& inGBufferData, EDebugTexture inDebugTexture) {
    return inRenderGraph.AddGraphicsPass<GBufferDebugData>("GBUFFER DEBUG PASS", inDevice,
    [&](IRenderPass* inRenderPass, GBufferDebugData& inData) 
    {
        const auto render_texture = inDevice.CreateTexture(Texture::Desc{
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET,
        }, L"GBUFFER_DEBUG");

        inData.mOutputTexture = inRenderPass->CreateAndWrite(render_texture);

        switch (inDebugTexture) {
            case DEBUG_TEXTURE_GBUFFER_DEPTH:     inData.mInputTexture = inGBufferData.mDepthTexture;  break;
            case DEBUG_TEXTURE_GBUFFER_ALBEDO:    inData.mInputTexture = inGBufferData.mRenderTexture; break;
            case DEBUG_TEXTURE_GBUFFER_NORMALS:   inData.mInputTexture = inGBufferData.mRenderTexture; break;
            case DEBUG_TEXTURE_GBUFFER_METALLIC:  inData.mInputTexture = inGBufferData.mRenderTexture; break;
            case DEBUG_TEXTURE_GBUFFER_ROUGHNESS: inData.mInputTexture = inGBufferData.mRenderTexture; break;
            default: assert(false);
        }

        inData.mInputTexture = inRenderPass->Read(inData.mInputTexture);

        CD3DX12_SHADER_BYTECODE vertex_shader, pixel_shader;
        switch (inDebugTexture) {
            case DEBUG_TEXTURE_GBUFFER_DEPTH:     g_SystemShaders.mGBufferDebugDepthShader.GetGraphicsProgram(vertex_shader, pixel_shader);      break;
            case DEBUG_TEXTURE_GBUFFER_ALBEDO:    g_SystemShaders.mGBufferDebugAlbedoShader.GetGraphicsProgram(vertex_shader, pixel_shader);     break;
            case DEBUG_TEXTURE_GBUFFER_NORMALS:   g_SystemShaders.mGBufferDebugNormalsShader.GetGraphicsProgram(vertex_shader, pixel_shader);    break;
            case DEBUG_TEXTURE_GBUFFER_METALLIC:  g_SystemShaders.mGBufferDebugMetallicShader.GetGraphicsProgram(vertex_shader, pixel_shader);   break;
            case DEBUG_TEXTURE_GBUFFER_ROUGHNESS: g_SystemShaders.mGBufferDebugRoughnessShader.GetGraphicsProgram(vertex_shader, pixel_shader);  break;
            default: assert(false);
        }

        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, vertex_shader, pixel_shader);
        state.InputLayout = {}; // clear the input layout, we generate the fullscreen triangle inside the vertex shader
        state.DepthStencilState.DepthEnable = FALSE;
        state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&inData.mPipeline));
        inData.mPipeline->SetName(L"PSO_GBUFFER_DEBUG");
    },
    [&inRenderGraph, &inDevice](GBufferDebugData& inData, CommandList& inCmdList) 
    {   
            const auto root_constants = GbufferDebugRootConstants {
                .mTexture   = inData.mInputTexture.GetBindlessIndex(inDevice),
                .mFarPlane  = inRenderGraph.GetViewport().GetCamera().GetFar(),
                .mNearPlane = inRenderGraph.GetViewport().GetCamera().GetNear(),
            };

            inCmdList->SetPipelineState(inData.mPipeline.Get());
            inCmdList.SetViewportScissorRect(inRenderGraph.GetViewport());
            inCmdList.PushGraphicsConstants(root_constants);
            inCmdList->DrawInstanced(6, 1, 0, 0);
    });
}



const RTShadowMaskData& AddShadowMaskPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferData& inGBufferData) {
    return inRenderGraph.AddComputePass<RTShadowMaskData>("RAY TRACED SHADOWS PASS", inDevice,
    [&](IRenderPass* inRenderPass, RTShadowMaskData& inData)
    {
        const auto output_texture = inDevice.CreateTexture(Texture::Desc{
            .format = DXGI_FORMAT_R32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::Usage::SHADER_READ_WRITE
        }, L"SHADOW_MASK");

        inRenderPass->Create(output_texture);

        inData.mOutputTexture                   = inRenderPass->Write(output_texture);
        inData.mGbufferDepthTexture             = inRenderPass->Read(inGBufferData.mDepthTexture);
        inData.mGBufferRenderTexture            = inRenderPass->Read(inGBufferData.mRenderTexture);
        inData.mTopLevelAccelerationStructure   = inScene.GetTLASDescriptor(inDevice);

        CD3DX12_SHADER_BYTECODE compute_shader;
        g_SystemShaders.mRTShadowsShader.GetComputeProgram(compute_shader);
        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, compute_shader);

        gThrowIfFailed(inDevice->CreateComputePipelineState(&state, IID_PPV_ARGS(&inData.mPipeline)));
        inData.mPipeline->SetName(L"PSO_RT_SHADOWS");
    },

    [&inRenderGraph, &inDevice](RTShadowMaskData& inData, CommandList& inCmdList)
    {
        auto& viewport = inRenderGraph.GetViewport();

        const auto root_constants = ShadowMaskRootConstants {
            .mGbufferRenderTexture  = inData.mGBufferRenderTexture.GetBindlessIndex(inDevice),
            .mGbufferDepthTexture   = inData.mGbufferDepthTexture.GetBindlessIndex(inDevice),
            .mShadowMaskTexture     = inData.mOutputTexture.GetBindlessIndex(inDevice),
            .mTLAS                  = inDevice.GetBindlessHeapIndex(inData.mTopLevelAccelerationStructure),
            .mDispatchSize          = viewport.size
        };

        inCmdList->SetComputeRoot32BitConstants(0, sizeof(ShadowMaskRootConstants) / sizeof(DWORD), &root_constants, 0);
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->Dispatch(viewport.size.x / 8, viewport.size.y / 8, 1);
    });
}



const RTAOData& AddAmbientOcclusionPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferData& inGbufferData) {
    return inRenderGraph.AddComputePass<RTAOData>("RAY TRACED AO PASS", inDevice,
    [&](IRenderPass* inRenderPass, RTAOData& inData) 
    {  
        const auto output_texture = inDevice.CreateTexture(Texture::Desc{
            .format = DXGI_FORMAT_R32_FLOAT,
            .width = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage = Texture::Usage::SHADER_READ_WRITE
        }, L"AO_MASK");

        inRenderPass->Create(output_texture);

        inData.mOutputTexture                   = inRenderPass->Write(output_texture);
        inData.mGbufferDepthTexture             = inRenderPass->Read(inGbufferData.mDepthTexture);
        inData.mGBufferRenderTexture            = inRenderPass->Read(inGbufferData.mRenderTexture);
        inData.mTopLevelAccelerationStructure   = inScene.GetTLASDescriptor(inDevice);

        CD3DX12_SHADER_BYTECODE compute_shader;
        g_SystemShaders.mRTAmbientOcclusionShader.GetComputeProgram(compute_shader);
        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, compute_shader);

        gThrowIfFailed(inDevice->CreateComputePipelineState(&state, IID_PPV_ARGS(&inData.mPipeline)));
        inData.mPipeline->SetName(L"PSO_RTAO");
    },
    [&inRenderGraph, &inDevice](RTAOData& inData, CommandList& inCmdList) 
    {
        auto& viewport = inRenderGraph.GetViewport();

        const auto root_constants = AmbientOcclusionRootConstants {
            .mParams                = inData.mParams,
            .mGbufferRenderTexture  = inData.mGBufferRenderTexture.GetBindlessIndex(inDevice),
            .mGbufferDepthTexture   = inData.mGbufferDepthTexture.GetBindlessIndex(inDevice),
            .mAOmaskTexture         = inData.mOutputTexture.GetBindlessIndex(inDevice),
            .mTLAS                  = inDevice.GetBindlessHeapIndex(inData.mTopLevelAccelerationStructure),
            .mDispatchSize          = viewport.size
        };

        inCmdList.PushComputeConstants(root_constants);
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->Dispatch(viewport.size.x / 8, viewport.size.y / 8, 1);
    });
}



const GrassData& AddGrassRenderPass(RenderGraph& inGraph, Device& inDevice, const GBufferData& inGBufferData)
{
    return inGraph.AddGraphicsPass<GrassData>("GRASS DRAW PASS", inDevice,
    [&](IRenderPass* inRenderPass, GrassData& inData)
    {
        inData.mDepthTexture  = inRenderPass->Write(inGBufferData.mDepthTexture);
        inData.mRenderTexture = inRenderPass->Write(inGBufferData.mRenderTexture);

        CD3DX12_SHADER_BYTECODE vertex_shader, pixel_shader;
        g_SystemShaders.mGrassShader.GetGraphicsProgram(vertex_shader, pixel_shader);
        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, vertex_shader, pixel_shader);
        
        pso_state.InputLayout = {};
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(&inData.mPipeline));
        inData.mPipeline->SetName(L"PSO_GRASS_DRAW");

        inData.mRenderConstants = GrassRenderRootConstants {
            .mBend          = 0.0f,
            .mTilt          = 0.0f,
            .mWindDirection = glm::vec2(0.0f, -1.0f)
        };
    },
    [](GrassData& inData, CommandList& inCmdList)
    {
        const auto blade_vertex_count = 15;

        inCmdList.PushGraphicsConstants(inData.mRenderConstants);

        inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->DrawInstanced(blade_vertex_count, 65536, 0, 0);
        inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    });
}



const ReflectionsData& AddReflectionsPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferData& inGBufferData) {
    return inRenderGraph.AddComputePass<ReflectionsData>("RT REFLECTION TRACE PASS", inDevice,
    [&](IRenderPass* inRenderPass, ReflectionsData& inData)
    {
        const auto result_texture = inDevice.CreateTexture(Texture::Desc{
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .mipLevels = 0, // let it auto-calculate the nr of mips
            .usage  = Texture::Usage::SHADER_READ_WRITE
        }, L"RT_REFLECTIONS");

        inRenderPass->Create(result_texture);

        inData.mOutputTexture = inRenderPass->Write(result_texture);
        inData.mGBufferDepthTexture = inRenderPass->Read(inGBufferData.mDepthTexture);
        inData.mGbufferRenderTexture = inRenderPass->Read(inGBufferData.mRenderTexture);
        inData.mTopLevelAccelerationStructure = inScene.GetTLASDescriptor(inDevice);
        inData.mInstancesBuffer = inScene.GetInstancesDescriptor(inDevice);
        inData.mMaterialBuffer = inScene.GetMaterialsDescriptor(inDevice);

        CD3DX12_SHADER_BYTECODE compute_shader;
        g_SystemShaders.mRTReflectionsShader.GetComputeProgram(compute_shader);
        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, compute_shader);
        
        Timer timer;
        gThrowIfFailed(inDevice->CreateComputePipelineState(&state, IID_PPV_ARGS(inData.mPipeline.GetAddressOf())));
        inData.mPipeline->SetName(L"PSO_RT_REFLECTIONS");
    },
    [&inRenderGraph, &inDevice](ReflectionsData& inData, CommandList& inCmdList) 
    {
        auto& viewport = inRenderGraph.GetViewport();

        const auto root_constants   = ReflectionsRootConstants {
            .mGbufferRenderTexture  = inData.mGbufferRenderTexture.GetBindlessIndex(inDevice),
            .mGbufferDepthTexture   = inData.mGBufferDepthTexture.GetBindlessIndex(inDevice),
            .mShadowMaskTexture     = inData.mOutputTexture.GetBindlessIndex(inDevice),
            .mTLAS                  = inDevice.GetBindlessHeapIndex(inData.mTopLevelAccelerationStructure),
            .mInstancesBuffer       = inDevice.GetBindlessHeapIndex(inData.mInstancesBuffer),
            .mMaterialsBuffer       = inDevice.GetBindlessHeapIndex(inData.mMaterialBuffer),
            .mDispatchSize          = viewport.size
        };

        inCmdList->SetComputeRoot32BitConstants(0, sizeof(ReflectionsRootConstants) / sizeof(DWORD), &root_constants, 0);
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->Dispatch(viewport.size.x / 8, viewport.size.y / 8, 1);
    });
}



const PathTraceData& AddPathTracePass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferData& inGBufferData) {
    return inRenderGraph.AddComputePass<PathTraceData>("PATH TRACE PASS", inDevice,
    [&](IRenderPass* inRenderPass, PathTraceData& inData)
    {
        const auto result_texture = inDevice.CreateTexture(Texture::Desc {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::Usage::SHADER_READ_WRITE
        }, L"RT_PATH_TRACED");

        inRenderPass->Create(result_texture);

        inData.mOutputTexture                   = inRenderPass->Write(result_texture);
        inData.mGBufferDepthTexture             = inRenderPass->Read(inGBufferData.mDepthTexture);
        inData.mGbufferRenderTexture            = inRenderPass->Read(inGBufferData.mRenderTexture);
        inData.mTopLevelAccelerationStructure   = inScene.GetTLASDescriptor(inDevice);
        inData.mInstancesBuffer                 = inScene.GetInstancesDescriptor(inDevice);
        inData.mMaterialBuffer                  = inScene.GetMaterialsDescriptor(inDevice);

        CD3DX12_SHADER_BYTECODE compute_shader;
        g_SystemShaders.mRTIndirectDiffuseShader.GetComputeProgram(compute_shader);
        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, compute_shader);

        gThrowIfFailed(inDevice->CreateComputePipelineState(&state, IID_PPV_ARGS(inData.mPipeline.GetAddressOf())));
        inData.mPipeline->SetName(L"PSO_RT_PATH_TRACE");
    },
    [&inRenderGraph, &inDevice](PathTraceData& inData, CommandList& inCmdList)
    {
        auto& viewport = inRenderGraph.GetViewport();

        const auto root_constants = PathTraceRootConstants {
            .mTLAS              = inDevice.GetBindlessHeapIndex(inData.mTopLevelAccelerationStructure),
            .mBounces           = inData.mBounces,
            .mInstancesBuffer   = inDevice.GetBindlessHeapIndex(inData.mInstancesBuffer),
            .mMaterialsBuffer   = inDevice.GetBindlessHeapIndex(inData.mMaterialBuffer),
            .mDispatchSize      = viewport.size,
            .mResultTexture     = inData.mOutputTexture.GetBindlessIndex(inDevice)
        };

        inCmdList.PushComputeConstants(root_constants);
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->Dispatch(viewport.size.x / 8, viewport.size.y / 8, 1);
    });
}



const DownsampleData& AddDownsamplePass(RenderGraph& inRenderGraph, Device& inDevice, const TextureResource& inSourceTexture) {
    return inRenderGraph.AddComputePass<DownsampleData>("DOWNSAMPLE PASS", inDevice,
    [&](IRenderPass* inRenderPass, DownsampleData& inData)
    {
        inData.mGlobalAtomicBuffer = inDevice.CreateBuffer(Buffer::Desc {
            .size   = sizeof(uint32_t),
            .stride = sizeof(uint32_t),
            .usage  = Buffer::Usage::SHADER_READ_WRITE
        });

        inData.mSourceTexture = inRenderPass->Write(inSourceTexture);

        const auto& texture = inDevice.GetTexture(inData.mSourceTexture.mCreatedTexture);
        auto texture_desc = texture.GetDesc();

        const auto nr_of_mips = gSpdCaculateMipCount(texture_desc.width, texture_desc.height);
        
        for (uint32_t mip = 0; mip < nr_of_mips; mip++) {
            auto srv_desc = D3D12_UNORDERED_ACCESS_VIEW_DESC {
                .Format = texture_desc.format,
                .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
                .Texture2D = D3D12_TEX2D_UAV {
                    .MipSlice = mip
                },
            };

            texture_desc.viewDesc = &srv_desc;
            inData.mTextureMips[mip] = inDevice.CreateTextureView(inSourceTexture.mResourceTexture, texture_desc);
        }

        CD3DX12_SHADER_BYTECODE compute_shader;
        g_SystemShaders.mDownsampleShader.GetComputeProgram(compute_shader);
        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, compute_shader);

        inDevice->CreateComputePipelineState(&state, IID_PPV_ARGS(inData.mPipeline.GetAddressOf()));
        inData.mPipeline->SetName(L"PSO_DOWNSAMPLE");
    },
    [&inRenderGraph, &inDevice](DownsampleData& inData, CommandList& inCmdList)
    {
        const auto& texture = inDevice.GetTexture(inData.mSourceTexture.mCreatedTexture);
        const auto rect_info = glm::uvec4(0u, 0u, texture->GetDesc().Width, texture->GetDesc().Height);

        glm::uvec2 work_group_offset, dispatchThreadGroupCountXY, numWorkGroupsAndMips;
        work_group_offset[0] = rect_info[0] / 64; // rectInfo[0] = left
        work_group_offset[1] = rect_info[1] / 64; // rectInfo[1] = top

        auto endIndexX = (rect_info[0] + rect_info[2] - 1) / 64; // rectInfo[0] = left, rectInfo[2] = width
        auto endIndexY = (rect_info[1] + rect_info[3] - 1) / 64; // rectInfo[1] = top, rectInfo[3] = height

        dispatchThreadGroupCountXY[0] = endIndexX + 1 - work_group_offset[0];
        dispatchThreadGroupCountXY[1] = endIndexY + 1 - work_group_offset[1];

        numWorkGroupsAndMips[0] = (dispatchThreadGroupCountXY[0]) * (dispatchThreadGroupCountXY[1]);
        numWorkGroupsAndMips[1] = gSpdCaculateMipCount(rect_info[2], rect_info[3]);

        const auto& atomic_buffer = inDevice.GetBuffer(inData.mGlobalAtomicBuffer);

        auto root_constants = SpdRootConstants {
            .mNrOfMips           = numWorkGroupsAndMips[1],
            .mNrOfWorkGroups     = numWorkGroupsAndMips[0],
            .mGlobalAtomicBuffer = inDevice.GetBindlessHeapIndex(atomic_buffer.GetDescriptor()),
            .mWorkGroupOffset    = work_group_offset,
        };

        const auto mips_ptr = &root_constants.mTextureMip0;

        for (auto mip = 0u; mip < numWorkGroupsAndMips[1]; mip++)
            mips_ptr[mip] = inDevice.GetBindlessHeapIndex(inData.mTextureMips[mip]);

        static auto first_run = true;

        if (first_run) {
            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(atomic_buffer.GetResource().Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);

            inCmdList->ResourceBarrier(1, &barrier);

            const auto param = D3D12_WRITEBUFFERIMMEDIATE_PARAMETER { .Dest = atomic_buffer->GetGPUVirtualAddress(), .Value = 0 };
            inCmdList->WriteBufferImmediate(1, &param, nullptr);

            barrier = CD3DX12_RESOURCE_BARRIER::Transition(atomic_buffer.GetResource().Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            inCmdList->ResourceBarrier(1, &barrier);

            first_run = false;
        }

        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.PushComputeConstants(root_constants);
        inCmdList->Dispatch(dispatchThreadGroupCountXY.x, dispatchThreadGroupCountXY.y, 1);
    });
}



const ProbeTraceData& AddProbeTracePass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene) {
    return inRenderGraph.AddComputePass<ProbeTraceData>("GI PROBE TRACE PASS", inDevice,
    [&](IRenderPass* inRenderPass, ProbeTraceData& inData)
    {
        const auto total_probe_count = inData.mDDGIData.mProbeCount.x * inData.mDDGIData.mProbeCount.y * inData.mDDGIData.mProbeCount.z;

        const auto rays_depth_texture = inDevice.CreateTexture(Texture::Desc{
            .format = DXGI_FORMAT_R16G16_FLOAT,
            .width  = DDGI_RAYS_PER_PROBE,
            .height = uint32_t(total_probe_count),
            .usage  = Texture::Usage::SHADER_READ_WRITE
        }, L"DDGI_TRACE_DEPTH");

        const auto rays_irradiance_texture = inDevice.CreateTexture(Texture::Desc{
            .format = DXGI_FORMAT_R11G11B10_FLOAT,
            .width  = DDGI_RAYS_PER_PROBE,
            .height = uint32_t(total_probe_count),
            .usage  = Texture::Usage::SHADER_READ_WRITE
        }, L"DDGI_TRACE_IRRADIANCE");

        inData.mRaysDepthTexture = inRenderPass->CreateAndWrite(rays_depth_texture);
        inData.mRaysIrradianceTexture = inRenderPass->CreateAndWrite(rays_irradiance_texture);

        inData.mInstancesBuffer = inScene.GetInstancesDescriptor(inDevice);
        inData.mMaterialBuffer = inScene.GetMaterialsDescriptor(inDevice);
        inData.mTopLevelAccelerationStructure = inScene.GetTLASDescriptor(inDevice);

        CD3DX12_SHADER_BYTECODE compute_shader;
        g_SystemShaders.mProbeTraceShader.GetComputeProgram(compute_shader);
        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, compute_shader);

        gThrowIfFailed(inDevice->CreateComputePipelineState(&pso_state, IID_PPV_ARGS(&inData.mPipeline)));
        inData.mPipeline->SetName(L"PSO_PROBE_TRACE");
    },
    [&inRenderGraph, &inDevice](ProbeTraceData& inData, CommandList& inCmdList)
    {
        auto Index3Dto1D = [](UVec3 inIndex, UVec3 inCount) {
            return inIndex.x + inIndex.y * inCount.x + inIndex.z * inCount.x * inCount.y;
        };
        
        inData.mRandomRotationMatrix              = gRandomOrientation();
        inData.mDDGIData.mRaysDepthTexture        = inData.mRaysDepthTexture.GetBindlessIndex(inDevice);
        inData.mDDGIData.mRaysIrradianceTexture   = inData.mRaysIrradianceTexture.GetBindlessIndex(inDevice);

        auto constants = ProbeTraceRootConstants {
            .mInstancesBuffer       = inDevice.GetBindlessHeapIndex(inData.mInstancesBuffer),
            .mMaterialsBuffer       = inDevice.GetBindlessHeapIndex(inData.mMaterialBuffer),
            .mTLAS                  = inDevice.GetBindlessHeapIndex(inData.mTopLevelAccelerationStructure),
            .mDebugProbeIndex       = Index3Dto1D(inData.mDebugProbe, inData.mDDGIData.mProbeCount),
            .mDDGIData              = inData.mDDGIData,
            .mRandomRotationMatrix  = inData.mRandomRotationMatrix
        };

        const auto total_probe_count = inData.mDDGIData.mProbeCount.x * inData.mDDGIData.mProbeCount.y * inData.mDDGIData.mProbeCount.z;

        inCmdList.PushComputeConstants(constants);
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->Dispatch(DDGI_RAYS_PER_PROBE / DDGI_WAVE_SIZE, total_probe_count, 1);
    });
}



const ProbeUpdateData& AddProbeUpdatePass(RenderGraph& inRenderGraph, Device& inDevice, const ProbeTraceData& inTraceData) {
    return inRenderGraph.AddComputePass<ProbeUpdateData>("GI PROBE UPDATE PASS", inDevice,
    [&](IRenderPass* inRenderPass, ProbeUpdateData& inData) 
    {
        inData.mDDGIData             = inTraceData.mDDGIData;
        const auto total_probe_count = inData.mDDGIData.mProbeCount.x * inData.mDDGIData.mProbeCount.y * inData.mDDGIData.mProbeCount.z;
     
        const auto probes_depth_texture = inDevice.CreateTexture(Texture::Desc{
            .format = DXGI_FORMAT_R16G16_FLOAT,
            .width  = uint32_t(DDGI_DEPTH_TEXELS * DDGI_PROBES_PER_ROW),
            .height = uint32_t(DDGI_DEPTH_TEXELS * (total_probe_count / DDGI_PROBES_PER_ROW)),
            .usage  = Texture::Usage::SHADER_READ_WRITE
        }, L"DDGI_UPDATE_DEPTH");

        const auto probes_irradiance_texture = inDevice.CreateTexture(Texture::Desc{
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = uint32_t(DDGI_IRRADIANCE_TEXELS * DDGI_PROBES_PER_ROW),
            .height = uint32_t(DDGI_IRRADIANCE_TEXELS * (total_probe_count / DDGI_PROBES_PER_ROW)),
            .usage  = Texture::Usage::SHADER_READ_WRITE
        }, L"DDGI_UPDATE_IRRADIANCE");

        inData.mProbesDepthTexture      = inRenderPass->CreateAndWrite(probes_depth_texture);
        inData.mProbesIrradianceTexture = inRenderPass->CreateAndWrite(probes_irradiance_texture);
        inData.mRaysDepthTexture        = inRenderPass->Read(inTraceData.mRaysDepthTexture);
        inData.mRaysIrradianceTexture   = inRenderPass->Read(inTraceData.mRaysIrradianceTexture);

        {
            CD3DX12_SHADER_BYTECODE compute_shader;
            g_SystemShaders.mProbeUpdateDepthShader.GetComputeProgram(compute_shader);
            auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, compute_shader);
            
            gThrowIfFailed(inDevice->CreateComputePipelineState(&pso_state, IID_PPV_ARGS(&inData.mDepthPipeline)));
            inData.mDepthPipeline->SetName(L"PSO_PROBE_UPDATE_DEPTH");
        }

        {
            CD3DX12_SHADER_BYTECODE compute_shader;
            g_SystemShaders.mProbeUpdateIrradianceShader.GetComputeProgram(compute_shader);
            auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, compute_shader);

            gThrowIfFailed(inDevice->CreateComputePipelineState(&pso_state, IID_PPV_ARGS(&inData.mIrradiancePipeline)));
            inData.mIrradiancePipeline->SetName(L"PSO_PROBE_UPDATE_IRRADIANCE");
        }

    },
    [&inDevice, &inTraceData](ProbeUpdateData& inData, CommandList& inCmdList)
    {
        inData.mDDGIData                          = inTraceData.mDDGIData;
        inData.mDDGIData.mProbesDepthTexture      = inData.mProbesDepthTexture.GetBindlessIndex(inDevice);
        inData.mDDGIData.mProbesIrradianceTexture = inData.mProbesIrradianceTexture.GetBindlessIndex(inDevice);
        inData.mDDGIData.mRaysDepthTexture        = inData.mRaysDepthTexture.GetBindlessIndex(inDevice);
        inData.mDDGIData.mRaysIrradianceTexture   = inData.mRaysIrradianceTexture.GetBindlessIndex(inDevice);

        const auto root_constants = ProbeUpdateRootConstants {
            .mDDGIData             = inData.mDDGIData,
            .mRandomRotationMatrix = inTraceData.mRandomRotationMatrix
        };

        inCmdList.PushComputeConstants(root_constants);

        //inCmdList->SetPipelineState(inData.mDepthPipeline.Get());
        //const auto depth_texture = inDevice.GetTexture(inData.mProbesDepthTexture.mCreatedTexture);
        //inCmdList->Dispatch(depth_texture.GetDesc().width / DDGI_DEPTH_TEXELS, depth_texture.GetDesc().height / DDGI_DEPTH_TEXELS, 1);

        inCmdList->SetPipelineState(inData.mIrradiancePipeline.Get());
        const auto irradiance_texture = inDevice.GetTexture(inData.mProbesIrradianceTexture.mCreatedTexture);
        inCmdList->Dispatch(irradiance_texture.GetDesc().width / DDGI_IRRADIANCE_TEXELS, irradiance_texture.GetDesc().height / DDGI_IRRADIANCE_TEXELS, 1);
    });
}



const ProbeDebugData& AddProbeDebugPass(RenderGraph& inRenderGraph, Device& inDevice, const ProbeTraceData& inTraceData, const ProbeUpdateData& inUpdateData, TextureResource inRenderTarget, TextureResource inDepthTarget) {
    return inRenderGraph.AddGraphicsPass<ProbeDebugData>("GI PROBE DEBUG PASS", inDevice,
    [&](IRenderPass* inRenderPass, ProbeDebugData& inData)
    {
        gGenerateSphere(inData.mProbeMesh, .25f, 16u, 16u);

        const auto vertices = inData.mProbeMesh.GetInterleavedVertices();
        const auto vertices_size = vertices.size() * sizeof(vertices[0]);
        const auto indices_size = inData.mProbeMesh.indices.size() * sizeof(inData.mProbeMesh.indices[0]);

        inData.mProbeMesh.indexBuffer = inDevice.CreateBuffer(Buffer::Desc{
            .size     = uint32_t(indices_size),
            .stride   = sizeof(uint32_t) * 3,
            .usage    = Buffer::Usage::INDEX_BUFFER,
            .mappable = true
        }).ToIndex();

        inData.mProbeMesh.vertexBuffer = inDevice.CreateBuffer(Buffer::Desc{
            .size     = uint32_t(vertices_size),
            .stride   = sizeof(Vertex),
            .usage    = Buffer::Usage::VERTEX_BUFFER,
            .mappable = true
        }).ToIndex();

        {
            auto& index_buffer  = inDevice.GetBuffer(BufferID(inData.mProbeMesh.indexBuffer));
            void* mapped_ptr = nullptr;
            index_buffer->Map(0, nullptr, &mapped_ptr);
            memcpy(mapped_ptr, inData.mProbeMesh.indices.data(), indices_size);
            index_buffer->Unmap(0, nullptr);
        }

        {
            auto& vertex_buffer = inDevice.GetBuffer(BufferID(inData.mProbeMesh.vertexBuffer));
            void* mapped_ptr = nullptr;
            vertex_buffer->Map(0, nullptr, &mapped_ptr);
            memcpy(mapped_ptr, vertices.data(), vertices_size);
            vertex_buffer->Unmap(0, nullptr);
        }

        inData.mDDGIData     = inUpdateData.mDDGIData;
        inData.mDepthTarget  = inRenderPass->Write(inDepthTarget);
        inData.mRenderTarget = inRenderPass->Write(inRenderTarget);

        inData.mProbesDepthTexture      = inRenderPass->Read(inUpdateData.mProbesDepthTexture);
        inData.mProbesIrradianceTexture = inRenderPass->Read(inUpdateData.mProbesIrradianceTexture);

        CD3DX12_SHADER_BYTECODE vertex_shader, pixel_shader;
        g_SystemShaders.mProbeDebugShader.GetGraphicsProgram(vertex_shader, pixel_shader);
        auto pso_desc = inDevice.CreatePipelineStateDesc(inRenderPass, vertex_shader, pixel_shader);
        
        gThrowIfFailed(inDevice->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(inData.mPipeline.GetAddressOf())));
        inData.mPipeline->SetName(L"PSO_PROBE_DEBUG");
    },
    [&inRenderGraph, &inDevice, &inUpdateData](ProbeDebugData& inData, CommandList& inCmdList)
    {
        const auto& viewport = inRenderGraph.GetViewport();
        inCmdList.SetViewportScissorRect(viewport);
        inCmdList->SetPipelineState(inData.mPipeline.Get());

        inData.mDDGIData                          = inUpdateData.mDDGIData;
        inData.mDDGIData.mProbesDepthTexture      = inData.mProbesDepthTexture.GetBindlessIndex(inDevice);
        inData.mDDGIData.mProbesIrradianceTexture = inData.mProbesIrradianceTexture.GetBindlessIndex(inDevice);

        const auto total_probe_count = inData.mDDGIData.mProbeCount.x * inData.mDDGIData.mProbeCount.y * inData.mDDGIData.mProbeCount.z;

        inCmdList.PushGraphicsConstants(inData.mDDGIData);
        inCmdList.BindVertexAndIndexBuffers(inDevice, inData.mProbeMesh);
        inCmdList->DrawIndexedInstanced(inData.mProbeMesh.indices.size(), total_probe_count, 0, 0, 0);
    });
}



const DebugLinesData& AddDebugLinesPass(RenderGraph& inRenderGraph, Device& inDevice, TextureResource inRenderTarget, TextureResource inDepthTarget) {
    return inRenderGraph.AddGraphicsPass<DebugLinesData>("DEBUG LINES PASS", inDevice,
    [&](IRenderPass* inRenderPass, DebugLinesData& inData)
    {
        const auto vertex_buffer = inDevice.CreateBuffer(Buffer::Desc {
            .size   = sizeof(float4) * UINT16_MAX,
            .stride = sizeof(float4),
            .usage  = Buffer::Usage::SHADER_READ_WRITE
        });

        auto args_srv_desc = D3D12_UNORDERED_ACCESS_VIEW_DESC {
            .Format = DXGI_FORMAT_R32_TYPELESS,
            .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
            .Buffer = D3D12_BUFFER_UAV {
                .NumElements = sizeof(D3D12_DRAW_ARGUMENTS) / 4,
                .Flags = D3D12_BUFFER_UAV_FLAG_RAW, 
            }
        };

        const auto args_buffer = inDevice.CreateBuffer(Buffer::Desc {
            .size  = sizeof(D3D12_DRAW_ARGUMENTS),
            .usage = Buffer::Usage::SHADER_READ_WRITE,
            .viewDesc = &args_srv_desc
        });

        const auto depth_target    = inRenderPass->Write(inDepthTarget);
        const auto render_target   = inRenderPass->Write(inRenderTarget);
        inData.mVertexBuffer       = inRenderPass->CreateAndRead(vertex_buffer);
        inData.mIndirectArgsBuffer = inRenderPass->CreateAndRead(args_buffer);
        
        auto indirect_args = D3D12_INDIRECT_ARGUMENT_DESC { 
            .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW
        };

        const auto cmdsig_desc = D3D12_COMMAND_SIGNATURE_DESC {
            .ByteStride         = sizeof(D3D12_DRAW_ARGUMENTS),
            .NumArgumentDescs   = 1,
            .pArgumentDescs     = &indirect_args
        };

        inDevice->CreateCommandSignature(&cmdsig_desc, nullptr, IID_PPV_ARGS(inData.mCommandSignature.GetAddressOf()));

        static constexpr auto vertex_layout = std::array {
            D3D12_INPUT_ELEMENT_DESC { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        CD3DX12_SHADER_BYTECODE vertex_shader, pixel_shader;
        g_SystemShaders.mDebugLinesShader.GetGraphicsProgram(vertex_shader, pixel_shader);
        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, vertex_shader, pixel_shader);

        pso_state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso_state.RasterizerState.AntialiasedLineEnable = true;
        pso_state.InputLayout.NumElements = uint32_t(vertex_layout.size());
        pso_state.InputLayout.pInputElementDescs = vertex_layout.data();

        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(inData.mPipeline.GetAddressOf()));
        inData.mPipeline->SetName(L"PSO_DEBUG_LINES");
    },
    [&inDevice](DebugLinesData& inData, CommandList& inCmdList) 
    {   
        auto args_buffer_resource     = inDevice.GetResourcePtr(inData.mIndirectArgsBuffer.mCreatedBuffer);
        auto vertices_buffer_resource = inDevice.GetResourcePtr(inData.mVertexBuffer.mCreatedBuffer);

        const auto args_entry_barrier     = CD3DX12_RESOURCE_BARRIER::Transition(args_buffer_resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        const auto vertices_entry_barrier = CD3DX12_RESOURCE_BARRIER::Transition(vertices_buffer_resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        const auto entry_barriers         = std::array { args_entry_barrier, vertices_entry_barrier };

        const auto args_exit_barrier     = CD3DX12_RESOURCE_BARRIER::Transition(args_buffer_resource, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        const auto vertices_exit_barrier = CD3DX12_RESOURCE_BARRIER::Transition(vertices_buffer_resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        const auto exit_barriers         = std::array { args_exit_barrier, vertices_exit_barrier };

        const auto& vertex_buffer = inDevice.GetBuffer(inData.mVertexBuffer.mCreatedBuffer);
        
        const auto vertex_view = D3D12_VERTEX_BUFFER_VIEW {
            .BufferLocation = vertex_buffer->GetGPUVirtualAddress(),
            .SizeInBytes    = uint32_t(vertex_buffer->GetDesc().Width),
            .StrideInBytes  = sizeof(float4)
        };
        
        inCmdList->ResourceBarrier(entry_barriers.size(), entry_barriers.data());
        inCmdList->IASetVertexBuffers(0, 1, &vertex_view);
        inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->ExecuteIndirect(inData.mCommandSignature.Get(), 1, args_buffer_resource, 0, nullptr, 0);
        inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        inCmdList->ResourceBarrier(exit_barriers.size(), exit_barriers.data());
    });
}



const LightingData& AddLightingPass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferData& inGBufferData, const RTShadowMaskData& inShadowMaskData, const ReflectionsData& inReflectionsData, const RTAOData& inAmbientOcclusionData, const ProbeUpdateData& inProbeData) {
    return inRenderGraph.AddGraphicsPass<LightingData>("DEFERRED LIGHTING PASS", inDevice,
    [&](IRenderPass* inRenderPass, LightingData& inData)
    {
        const auto output_texture = inDevice.CreateTexture({
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET
        }, L"LIGHT_OUTPUT");

        inRenderPass->Create(output_texture);

        inData.mDDGIData                = inProbeData.mDDGIData;
        inData.mOutputTexture           = inRenderPass->Write(output_texture);
        inData.mShadowMaskTexture       = inRenderPass->Read(inShadowMaskData.mOutputTexture);
        inData.mReflectionsTexture      = inRenderPass->Read(inReflectionsData.mOutputTexture);
        inData.mGBufferDepthTexture     = inRenderPass->Read(inGBufferData.mDepthTexture);
        inData.mGBufferRenderTexture    = inRenderPass->Read(inGBufferData.mRenderTexture);
        inData.mAmbientOcclusionTexture = inRenderPass->Read(inAmbientOcclusionData.mOutputTexture);
        inData.mProbesDepthTexture      = inRenderPass->Read(inProbeData.mProbesDepthTexture);
        inData.mProbesIrradianceTexture = inRenderPass->Read(inProbeData.mProbesIrradianceTexture);
        // inData.mIndirectDiffuseTexture  = inRenderPass->Read(inDiffuseGIData.mOutputTexture);

        CD3DX12_SHADER_BYTECODE vertex_shader, pixel_shader;
        g_SystemShaders.mLightingShader.GetGraphicsProgram(vertex_shader, pixel_shader);
        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, vertex_shader, pixel_shader);

        pso_state.InputLayout = {}; // clear the input layout, we generate the fullscreen triangle inside the vertex shader
        pso_state.DepthStencilState.DepthEnable = FALSE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(&inData.mPipeline));
        inData.mPipeline->SetName(L"PSO_DEFERRED_LIGHTING");
    },
    [&inRenderGraph, &inDevice, &inProbeData](LightingData& inData, CommandList& inCmdList)
    {
        auto root_constants = LightingRootConstants {
            .mShadowMaskTexture         = inData.mShadowMaskTexture.GetBindlessIndex(inDevice),
            .mReflectionsTexture        = inData.mReflectionsTexture.GetBindlessIndex(inDevice),
            .mGbufferDepthTexture       = inData.mGBufferDepthTexture.GetBindlessIndex(inDevice),
            .mGbufferRenderTexture      = inData.mGBufferRenderTexture.GetBindlessIndex(inDevice),
            .mAmbientOcclusionTexture   = inData.mAmbientOcclusionTexture.GetBindlessIndex(inDevice),
            // .mIndirectDiffuseTexture    = inData.mIndirectDiffuseTexture.GetBindlessIndex(inDevice),
            .mDDGIData                  = inProbeData.mDDGIData
        };

        root_constants.mDDGIData.mProbesDepthTexture = inData.mProbesDepthTexture.GetBindlessIndex(inDevice);
        root_constants.mDDGIData.mProbesIrradianceTexture = inData.mProbesIrradianceTexture.GetBindlessIndex(inDevice);

        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.SetViewportScissorRect(inRenderGraph.GetViewport());
        inCmdList.PushGraphicsConstants(root_constants);
        inCmdList->DrawInstanced(6, 1, 0, 0);
    });
}



const FSR2Data& AddFsrPass(RenderGraph& inRenderGraph, Device& inDevice, FfxFsr2Context& inContext, TextureResource inColorTexture, const GBufferData& inGBufferData) {
    return inRenderGraph.AddComputePass<FSR2Data>("FSR2 PASS", inDevice,
    [&](IRenderPass* inRenderPass, FSR2Data& inData)
    {
        const auto output_texture = inDevice.CreateTexture(Texture::Desc{
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage = Texture::SHADER_READ_WRITE,
        }, L"FSR2_OUTPUT");

        inRenderPass->Create(output_texture);
        inData.mOutputTexture = inRenderPass->Write(output_texture);

        inData.mColorTexture        = inRenderPass->Read(inColorTexture);
        inData.mDepthTexture        = inRenderPass->Read(inGBufferData.mDepthTexture);
        inData.mMotionVectorTexture = inRenderPass->Read(inGBufferData.mMotionVectorTexture);

        inRenderPass->SetExternal(true);
    },

    [&inRenderGraph, &inDevice, &inContext](FSR2Data& inData, CommandList& inCmdList)
    {
        auto& viewport = inRenderGraph.GetViewport();

        const auto color_texture_ptr  = inDevice.GetResourcePtr(inData.mColorTexture.mCreatedTexture);
        const auto depth_texture_ptr  = inDevice.GetResourcePtr(inData.mDepthTexture.mCreatedTexture);
        const auto movec_texture_ptr  = inDevice.GetResourcePtr(inData.mMotionVectorTexture.mCreatedTexture);
        const auto output_texture_ptr = inDevice.GetResourcePtr(inData.mOutputTexture.mCreatedTexture);

        auto fsr2_dispatch_desc = FfxFsr2DispatchDescription{};
        fsr2_dispatch_desc.commandList              = ffxGetCommandListDX12(inCmdList);
        fsr2_dispatch_desc.color                    = ffxGetResourceDX12(&inContext, color_texture_ptr,  nullptr, FFX_RESOURCE_STATE_COMPUTE_READ);
        fsr2_dispatch_desc.depth                    = ffxGetResourceDX12(&inContext, depth_texture_ptr,  nullptr, FFX_RESOURCE_STATE_COMPUTE_READ);
        fsr2_dispatch_desc.motionVectors            = ffxGetResourceDX12(&inContext, movec_texture_ptr,  nullptr, FFX_RESOURCE_STATE_COMPUTE_READ);
        fsr2_dispatch_desc.output                   = ffxGetResourceDX12(&inContext, output_texture_ptr, nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
        fsr2_dispatch_desc.exposure                 = ffxGetResourceDX12(&inContext, nullptr);
        fsr2_dispatch_desc.motionVectorScale.x      = float(viewport.size.x);
        fsr2_dispatch_desc.motionVectorScale.y      = float(viewport.size.y);
        fsr2_dispatch_desc.reset                    = false;
        fsr2_dispatch_desc.enableSharpening         = false;
        fsr2_dispatch_desc.sharpness                = 0.0f;
        fsr2_dispatch_desc.frameTimeDelta           = Timer::sToMilliseconds(inData.mDeltaTime);
        fsr2_dispatch_desc.preExposure              = 1.0f;
        fsr2_dispatch_desc.renderSize.width         = viewport.size.x;
        fsr2_dispatch_desc.renderSize.height        = viewport.size.y;
        fsr2_dispatch_desc.cameraFar                = viewport.GetCamera().GetFar();
        fsr2_dispatch_desc.cameraNear               = viewport.GetCamera().GetNear();
        fsr2_dispatch_desc.cameraFovAngleVertical   = glm::radians(viewport.GetFieldOfView());

        const auto jitter_phase_count = ffxFsr2GetJitterPhaseCount(viewport.size.x, viewport.size.x);
        ffxFsr2GetJitterOffset(&fsr2_dispatch_desc.jitterOffset.x, &fsr2_dispatch_desc.jitterOffset.y, inData.mFrameCounter, jitter_phase_count);
        inData.mFrameCounter = (inData.mFrameCounter + 1) % jitter_phase_count;

        gThrowIfFailed(ffxFsr2ContextDispatch(&inContext, &fsr2_dispatch_desc));
    });
}



const ComposeData& AddComposePass(RenderGraph& inRenderGraph, Device& inDevice, TextureResource inInputTexture) {
    return inRenderGraph.AddGraphicsPass<ComposeData>("COMPOSE PASS", inDevice,
    [&](IRenderPass* inRenderPass, ComposeData& inData)
    {
        const auto output_texture = inDevice.CreateTexture(Texture::Desc{
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET,
        }, L"COMPOSE_OUTPUT");

        inRenderPass->Create(output_texture);
        inData.mOutputTexture = inRenderPass->Write(output_texture);
        inData.mInputTexture  = inRenderPass->Read(inInputTexture);

        CD3DX12_SHADER_BYTECODE vertex_shader, pixel_shader;
        g_SystemShaders.mFinalComposeShader.GetGraphicsProgram(vertex_shader, pixel_shader);
        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, vertex_shader, pixel_shader);

        pso_state.InputLayout = {}; // clear the input layout, we generate the fullscreen triangle inside the vertex shader
        pso_state.DepthStencilState.DepthEnable = FALSE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(&inData.mPipeline));
        inData.mPipeline->SetName(L"PSO_COMPOSE");
    },

    [&inRenderGraph, &inDevice](ComposeData& inData, CommandList& inCmdList)
    {
        struct {
            uint32_t mInputTexture;
        } root_constants;

        root_constants.mInputTexture = inData.mInputTexture.GetBindlessIndex(inDevice);
        inCmdList->SetPipelineState(inData.mPipeline.Get());

        inCmdList.SetViewportScissorRect(inRenderGraph.GetViewport());
        
        inCmdList->SetGraphicsRoot32BitConstants(0, sizeof(root_constants) / sizeof(DWORD), &root_constants, 0);
        inCmdList->DrawInstanced(6, 1, 0, 0);
    });
}



const PreImGuiData& AddPreImGuiPass(RenderGraph& inRenderGraph, Device& inDevice, TextureResource ioDisplayTexture) {
    return inRenderGraph.AddGraphicsPass<PreImGuiData>("COMPOSE PASS", inDevice,
        [&](IRenderPass* inRenderPass, PreImGuiData& inData)
        {
            // this pass only exists to transition the final texture to a read state so ImGui can access it TODO: FIXME: pls fix
            inData.mDisplayTexture = inRenderPass->Read(ioDisplayTexture);
        },

        [&inRenderGraph, &inDevice](PreImGuiData& inData, CommandList& inCmdList) { /* nothing to do. */ });
}



static void ImGui_ImplDX12_SetupRenderState(ImDrawData* draw_data, CommandList& inCmdList, const Buffer& inVertexBuffer, const Buffer& inIndexBuffer, ImGuiRootConstants& inRootConstants)
{
    // Setup orthographic projection matrix
    inRootConstants.mProjection = glm::ortho (
        draw_data->DisplayPos.x,
        draw_data->DisplayPos.x + draw_data->DisplaySize.x,
        draw_data->DisplayPos.y + draw_data->DisplaySize.y,
        draw_data->DisplayPos.y
    );

    // Setup viewport
    const auto vp = CD3DX12_VIEWPORT(0.0f, 0.0f, draw_data->DisplaySize.x, draw_data->DisplaySize.y);
    inCmdList->RSSetViewports(1, &vp);

    // Bind shader and vertex buffers
    unsigned int stride = sizeof(ImDrawVert);
    unsigned int offset = 0;
    
    D3D12_VERTEX_BUFFER_VIEW vbv;
    memset(&vbv, 0, sizeof(D3D12_VERTEX_BUFFER_VIEW));
    vbv.BufferLocation = inVertexBuffer->GetGPUVirtualAddress() + offset;
    vbv.SizeInBytes = inVertexBuffer.GetDesc().size;
    vbv.StrideInBytes = stride;
    inCmdList->IASetVertexBuffers(0, 1, &vbv);
    
    D3D12_INDEX_BUFFER_VIEW ibv;
    memset(&ibv, 0, sizeof(D3D12_INDEX_BUFFER_VIEW));
    ibv.BufferLocation = inIndexBuffer->GetGPUVirtualAddress();
    ibv.SizeInBytes = inIndexBuffer.GetDesc().size;
    ibv.Format = sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    inCmdList->IASetIndexBuffer(&ibv);

    // Setup blend factor
    const float blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };
    inCmdList->OMSetBlendFactor(blend_factor);
}



const ImGuiData& AddImGuiPass(RenderGraph& inRenderGraph, Device& inDevice, StagingHeap& inStagingHeap, TextureResource inInputTexture) {
    return inRenderGraph.AddGraphicsPass<ImGuiData>("IMGUI PASS", inDevice,
    [&](IRenderPass* inRenderPass, ImGuiData& inData)
    {
        constexpr size_t max_buffer_size = 65536; // Taken from Vulkan's ImGuiPass
        inData.mIndexScratchBuffer.resize(max_buffer_size);
        inData.mVertexScratchBuffer.resize(max_buffer_size);

        inData.mIndexBuffer = inDevice.CreateBuffer(Buffer::Desc{
            .size   = max_buffer_size,
            .stride = sizeof(uint16_t),
            .usage  = Buffer::Usage::INDEX_BUFFER
        }, L"IMGUI_INDEX_BUFFER");

        inData.mVertexBuffer = inDevice.CreateBuffer(Buffer::Desc{
            .size   = max_buffer_size,
            .stride = sizeof(ImDrawVert),
            .usage  = Buffer::Usage::VERTEX_BUFFER
        }, L"IMGUI_VERTEX_BUFFER");

        inData.mInputTexture = inRenderPass->Read(inInputTexture);
        const auto backbuffer = inRenderPass->Write(inRenderGraph.GetBackBuffer());
        assert(backbuffer.mResourceTexture == inRenderGraph.GetBackBuffer()); // backbuffer created with RTV, so Write should just return that

        static constexpr auto input_layout = std::array {
            D3D12_INPUT_ELEMENT_DESC { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ImDrawVert, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            D3D12_INPUT_ELEMENT_DESC { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ImDrawVert, uv),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            D3D12_INPUT_ELEMENT_DESC { "COLOR",    0, DXGI_FORMAT_R32_UINT,     0, offsetof(ImDrawVert, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        CD3DX12_SHADER_BYTECODE vertex_shader, pixel_shader;
        g_SystemShaders.mImGuiShader.GetGraphicsProgram(vertex_shader, pixel_shader);
        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, vertex_shader, pixel_shader);

        pso_state.InputLayout = D3D12_INPUT_LAYOUT_DESC {
            .pInputElementDescs = input_layout.data(),
            .NumElements = input_layout.size(),
        };
        pso_state.DepthStencilState.DepthEnable = FALSE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(&inData.mPipeline));
        inData.mPipeline->SetName(L"PSO_IMGUI");
    },

    [&inRenderGraph, &inDevice, &inStagingHeap](ImGuiData& inData, CommandList& inCmdList)
    {
        const auto backbuffer_id = inRenderGraph.GetBackBuffer();

        {   // manual barriers around the imported backbuffer resource, the rendergraph doesn't handle this kind of state
            auto backbuffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetResourcePtr(backbuffer_id), D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
            inCmdList->ResourceBarrier(1, &backbuffer_barrier);
        }

        inCmdList->SetPipelineState(inData.mPipeline.Get());

        auto draw_data = ImGui::GetDrawData();
        auto idx_dst = (ImDrawIdx*)inData.mIndexScratchBuffer.data();
        auto vtx_dst = (ImDrawVert*)inData.mVertexScratchBuffer.data();

        for (auto& cmd_list : Slice(draw_data->CmdLists, draw_data->CmdListsCount)) {
            memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtx_dst += cmd_list->VtxBuffer.Size;
            idx_dst += cmd_list->IdxBuffer.Size;
        }

        const auto index_buffer_size = (uint8_t*)idx_dst - inData.mIndexScratchBuffer.data();
        const auto vertex_buffer_size = (uint8_t*)vtx_dst - inData.mVertexScratchBuffer.data();

        assert(index_buffer_size < inData.mIndexScratchBuffer.size());
        assert(vertex_buffer_size < inData.mVertexScratchBuffer.size());

        auto nr_of_barriers = 0;
        auto barriers = std::array {
            D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetBuffer(inData.mIndexBuffer).GetResource().Get(), D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST)),
            D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetBuffer(inData.mVertexBuffer).GetResource().Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST))
        };

        if (index_buffer_size) nr_of_barriers++;
        if (vertex_buffer_size) nr_of_barriers++;

        if (nr_of_barriers)
            inCmdList->ResourceBarrier(nr_of_barriers, barriers.data());

        if (vertex_buffer_size)
            inStagingHeap.StageBuffer(inCmdList, inDevice.GetBuffer(inData.mVertexBuffer).GetResource(), 0, inData.mVertexScratchBuffer.data(), vertex_buffer_size);

        if (index_buffer_size)
            inStagingHeap.StageBuffer(inCmdList, inDevice.GetBuffer(inData.mIndexBuffer).GetResource(), 0, inData.mIndexScratchBuffer.data(), index_buffer_size);

        if (nr_of_barriers) {
            for (auto& barrier : barriers)
                std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
            inCmdList->ResourceBarrier(nr_of_barriers, barriers.data());
        }
        
        auto root_constants = ImGuiRootConstants{};
        root_constants.mBindlessTextureIndex = 1;
        ImGui_ImplDX12_SetupRenderState(draw_data, inCmdList, inDevice.GetBuffer(inData.mVertexBuffer), inDevice.GetBuffer(inData.mIndexBuffer), root_constants);

        int global_vtx_offset = 0;
        int global_idx_offset = 0;
        ImVec2 clip_off = draw_data->DisplayPos;

        for (auto& cmd_list : Slice(draw_data->CmdLists, draw_data->CmdListsCount)) {
            for (const auto& cmd : cmd_list->CmdBuffer) {
                if (cmd.UserCallback) {
                    if (cmd.UserCallback == ImDrawCallback_ResetRenderState)
                        ImGui_ImplDX12_SetupRenderState(draw_data, inCmdList, inDevice.GetBuffer(inData.mVertexBuffer), inDevice.GetBuffer(inData.mIndexBuffer), root_constants);
                    else
                        cmd.UserCallback(cmd_list, &cmd);
                }
                else {
                    // IRenderer::GetImGuiTextureID writes out the bindless index into the ResourceDescriptorHeap, so we can assign that directly here
                    if (cmd.TextureId)
                        root_constants.mBindlessTextureIndex = 1; 
                    else
                        root_constants.mBindlessTextureIndex = 1; // set 0 to make debugging easier (should be the blue noise texture I think)

                    inCmdList.PushGraphicsConstants(root_constants);

                    // Project scissor/clipping rectangles into framebuffer space
                    auto clip_min = ImVec2(cmd.ClipRect.x - clip_off.x, cmd.ClipRect.y - clip_off.y);
                    auto clip_max = ImVec2(cmd.ClipRect.z - clip_off.x, cmd.ClipRect.w - clip_off.y);
                    if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                        continue;

                    const D3D12_RECT scissor_rect = { (LONG)clip_min.x, (LONG)clip_min.y, (LONG)clip_max.x, (LONG)clip_max.y };
                    inCmdList->RSSetScissorRects(1, &scissor_rect);
                    
                    inCmdList->DrawIndexedInstanced(cmd.ElemCount, 1, cmd.IdxOffset + global_idx_offset, cmd.VtxOffset + global_vtx_offset, 0);
                }
            }
            global_idx_offset += cmd_list->IdxBuffer.Size;
            global_vtx_offset += cmd_list->VtxBuffer.Size;
        }

        {
            auto backbuffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetResourcePtr(backbuffer_id), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON);
            inCmdList->ResourceBarrier(1, &backbuffer_barrier);
        }

    });
}



TextureID InitImGui(Device& inDevice, DXGI_FORMAT inRtvFormat, uint32_t inFrameCount) {
    auto width = 0, height = 0;
    auto pixels = static_cast<unsigned char*>(nullptr);
    ImGui::GetIO().Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

    auto font_texture_id = inDevice.CreateTexture(Texture::Desc{
        .format = DXGI_FORMAT_R8_UNORM,
        .width  = uint32_t(width),
        .height = uint32_t(height),
        .usage  = Texture::SHADER_READ_ONLY
    }, L"IMGUI_FONT_TEXTURE");

    auto font_texture_view = inDevice.GetTexture(font_texture_id).GetView();
    const auto& descriptor_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    ImGui_ImplDX12_Init(
        inDevice,
        inFrameCount,
        inRtvFormat,
        descriptor_heap.GetHeap(),
        descriptor_heap.GetCPUDescriptorHandle(font_texture_view),
        descriptor_heap.GetGPUDescriptorHandle(font_texture_view)
    );

    ImGui_ImplDX12_CreateDeviceObjects();

    //auto imgui_id = (void*)(intptr_t)inDevice.GetBindlessHeapIndex(font_texture_id);
    //ImGui::GetIO().Fonts->SetTexID(imgui_id);

    return font_texture_id;
}



void RenderImGui(RenderGraph& inRenderGraph, Device& inDevice, CommandList& inCmdList) {
    PIXScopedEvent(static_cast<ID3D12GraphicsCommandList*>(inCmdList), PIX_COLOR(0, 255, 0), "IMGUI BACKEND PASS");

    // Just in-case we did some external pass like FSR2 before this that sets its own descriptor heaps
    inDevice.BindDrawDefaults(inCmdList);

    {   // manual barriers around the imported backbuffer resource, the rendergraph doesn't handle this kind of state
        auto backbuffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetResourcePtr(inRenderGraph.GetBackBuffer()), D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
        inCmdList->ResourceBarrier(1, &backbuffer_barrier);
    }

    const auto bb_viewport = CD3DX12_VIEWPORT(inDevice.GetTexture(inRenderGraph.GetBackBuffer()).GetResource().Get());
    const auto bb_scissor = CD3DX12_RECT(bb_viewport.TopLeftX, bb_viewport.TopLeftY, bb_viewport.Width, bb_viewport.Height);

    inCmdList->RSSetViewports(1, &bb_viewport);
    inCmdList->RSSetScissorRects(1, &bb_scissor);

    const auto& rtv_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    const auto rtv = std::array { rtv_heap.GetCPUDescriptorHandle(inDevice.GetTexture(inRenderGraph.GetBackBuffer()).GetView()) };

    inCmdList->OMSetRenderTargets(rtv.size(), rtv.data(), FALSE, nullptr);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), inCmdList);

    {
        auto backbuffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetResourcePtr(inRenderGraph.GetBackBuffer()), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON);
        inCmdList->ResourceBarrier(1, &backbuffer_barrier);
    }
}

} // raekor
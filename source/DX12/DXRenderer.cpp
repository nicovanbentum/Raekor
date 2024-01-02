#include "pch.h"
#include "DXRenderer.h"

#include "shared.h"
#include "DXScene.h"
#include "DXShader.h"
#include "DXRenderGraph.h"
#include "DXRenderPasses.h"

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

    auto factory = ComPtr<IDXGIFactory4> {};
    gThrowIfFailed(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory)));

    auto wmInfo = SDL_SysWMinfo {};
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(inWindow, &wmInfo);
    auto hwnd = wmInfo.info.win.window;

    auto swapchain = ComPtr<IDXGISwapChain1> {};
    gThrowIfFailed(factory->CreateSwapChainForHwnd(inDevice.GetGraphicsQueue(), hwnd, &swapchain_desc, nullptr, nullptr, &swapchain));
    gThrowIfFailed(swapchain.As(&m_Swapchain));

    // Disables DXGI's automatic ALT+ENTER and PRINTSCREEN
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_WINDOW_CHANGES);

    SDL_DisplayMode mode;
    SDL_GetDisplayMode(SDL_GetWindowDisplayIndex(m_Window), m_Settings.mDisplayRes, &mode);
    SDL_SetWindowDisplayMode(m_Window, &mode);

    m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

    for (const auto& [index, backbuffer_data] : gEnumerate(m_BackBufferData))
    {
        auto rtv_resource = D3D12ResourceRef(nullptr);
        gThrowIfFailed(m_Swapchain->GetBuffer(index, IID_PPV_ARGS(rtv_resource.GetAddressOf())));
        backbuffer_data.mBackBuffer = inDevice.CreateTextureView(rtv_resource, Texture::Desc{.usage = Texture::Usage::RENDER_TARGET });

        backbuffer_data.mDirectCmdList = CommandList(inDevice, D3D12_COMMAND_LIST_TYPE_DIRECT, index);
        backbuffer_data.mDirectCmdList->SetName(L"Raekor::DX12::CommandList(DIRECT)");

        backbuffer_data.mCopyCmdList = CommandList(inDevice, D3D12_COMMAND_LIST_TYPE_DIRECT, index);
        backbuffer_data.mCopyCmdList->SetName(L"Raekor::DX12::CommandList(COPY)");

        rtv_resource->SetName(L"BACKBUFFER");
    }

    inDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));
    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (!m_FenceEvent)
        gThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
}



void Renderer::OnResize(Device& inDevice, Viewport& inViewport, bool inFullScreen)
{
    for (auto& bb_data : m_BackBufferData)
        inDevice.ReleaseTextureImmediate(bb_data.mBackBuffer);

    auto desc = DXGI_SWAP_CHAIN_DESC {};
    gThrowIfFailed(m_Swapchain->GetDesc(&desc));

    auto window_width = 0, window_height = 0;
    SDL_GetWindowSize(m_Window, &window_width, &window_height);
    gThrowIfFailed(m_Swapchain->ResizeBuffers(desc.BufferCount, window_width, window_height, desc.BufferDesc.Format, desc.Flags));

    for (const auto& [index, backbuffer_data] : gEnumerate(m_BackBufferData))
    {
        auto rtv_resource = D3D12ResourceRef(nullptr);
        gThrowIfFailed(m_Swapchain->GetBuffer(index, IID_PPV_ARGS(rtv_resource.GetAddressOf())));

        backbuffer_data.mBackBuffer = inDevice.CreateTextureView(rtv_resource, Texture::Desc{.usage = Texture::Usage::RENDER_TARGET });
        rtv_resource->SetName(L"BACKBUFFER");
    }

    m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();


    if (!m_Settings.mEnableTAA && m_Settings.mUpscaler != UPSCALER_NONE && m_Settings.mUpscaleQuality < UPSCALER_QUALITY_COUNT)
    {
        inViewport.SetRenderSize(Upscaler::sGetRenderResolution(inViewport.GetDisplaySize(), EUpscalerQuality(m_Settings.mUpscaleQuality)));

        auto upscale_init_success = true;

        switch (m_Settings.mUpscaler)
        {
            case UPSCALER_FSR:  upscale_init_success = InitFSR(inDevice,  inViewport); break;
            case UPSCALER_DLSS: upscale_init_success = InitDLSS(inDevice, inViewport); break;
            case UPSCALER_XESS: upscale_init_success = InitXeSS(inDevice, inViewport); break;
            default: break;
        }

        assert(upscale_init_success);
    }
    else
    {
        // might be redundant but eh, make sure render size and display size match
        inViewport.SetRenderSize(inViewport.GetDisplaySize());
    }

    m_Settings.mFullscreen = inFullScreen;
}



void Renderer::OnRender(Application* inApp, Device& inDevice, Viewport& inViewport, RayTracedScene& inScene, StagingHeap& inStagingHeap, IRenderInterface* inRenderInterface, float inDeltaTime)
{
    // Check if any of the shader sources were updated and recompile them if necessary.
    // the OS file stamp checks are expensive so we only turn this on in debug builds.
    static auto force_hotload = OS::sCheckCommandLineOption("-force_enable_hotload");
    auto need_recompile = IF_DEBUG_ELSE(g_SystemShaders.HotLoad(inDevice), force_hotload ? g_SystemShaders.HotLoad(inDevice) : false);
    if (need_recompile)
        std::cout << std::format("Hotloaded system shaders.\n");

    static auto do_stress_test = OS::sCheckCommandLineOption("-stress_test");

    if (m_ShouldResize || need_recompile || (do_stress_test && m_FrameCounter > 0))
    {
        // Make sure nothing is using render targets anymore
        WaitForIdle(inDevice);

        // Resize the renderer, which recreates the swapchain backbuffers and re-inits upscalers
        OnResize(inDevice, inViewport, SDL_IsWindowExclusiveFullscreen(m_Window));

        // Recompile the renderer, super overkill. TODO: pls fix
        Recompile(inDevice, inScene, inRenderInterface);

        // Unflag
        m_ShouldResize = false;

        return;
    }

    // At this point in the frame we really need the previous frame's present job to have finished
    if (m_PresentJobPtr)
        m_PresentJobPtr->WaitCPU();

    auto& backbuffer_data = GetBackBufferData();
    auto  completed_value = m_Fence->GetCompletedValue();

    // make sure the backbuffer data we're about to use is no longer being used by the GPU
    if (completed_value < backbuffer_data.mFenceValue)
    {
        gThrowIfFailed(m_Fence->SetEventOnCompletion(backbuffer_data.mFenceValue, m_FenceEvent));
        WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
    }

    // at this point we know the GPU is no longer working on this frame, so free/release stuff here
     if (m_FrameCounter > 0)
    {
        inStagingHeap.RetireBuffers(backbuffer_data.mCopyCmdList);
        inStagingHeap.RetireBuffers(backbuffer_data.mDirectCmdList);
        backbuffer_data.mCopyCmdList.ReleaseTrackedResources();
        backbuffer_data.mDirectCmdList.ReleaseTrackedResources();
    }

    // Update the total running time of the application / renderer
    m_ElapsedTime += inDeltaTime;

    // Calculate a jittered projection matrix for TAA/FSR/DLSS/XESS
    const auto& camera = m_RenderGraph.GetViewport().GetCamera();
    const auto jitter_phase_count = ffxFsr2GetJitterPhaseCount(m_RenderGraph.GetViewport().GetRenderSize().x, m_RenderGraph.GetViewport().GetDisplaySize().x);

    float jitter_offset_x = 0;
    float jitter_offset_y = 0;
    ffxFsr2GetJitterOffset(&jitter_offset_x, &jitter_offset_y, m_FrameCounter, jitter_phase_count);

    const auto jitter_x = 2.0f * jitter_offset_x / (float)m_RenderGraph.GetViewport().GetRenderSize().x;
    const auto jitter_y = -2.0f * jitter_offset_y / (float)m_RenderGraph.GetViewport().GetRenderSize().y;
    const auto jitter_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(jitter_x, jitter_y, 0));
    
    const auto enable_jitter = m_Settings.mEnableTAA || m_Settings.mUpscaler;
    auto final_proj_matrix = enable_jitter ? jitter_matrix * camera.GetProjection() : camera.GetProjection();

    // Update all the frame constants and copy it in into the GPU ring buffer
    m_FrameConstants.mTime = m_ElapsedTime;
    m_FrameConstants.mDeltaTime = inDeltaTime;
    m_FrameConstants.mSunConeAngle = m_Settings.mSunConeAngle;
    m_FrameConstants.mFrameIndex = m_FrameCounter % sFrameCount;
    m_FrameConstants.mFrameCounter = m_FrameCounter;
    m_FrameConstants.mPrevJitter = m_FrameConstants.mJitter;
    m_FrameConstants.mJitter = enable_jitter ? Vec2(jitter_x, jitter_y) : Vec2(0.0f, 0.0f);
    m_FrameConstants.mSunColor = inScene->GetSunLight() ? inScene->GetSunLight()->GetColor() : Vec4(1.0f);
    m_FrameConstants.mSunDirection = Vec4(inScene->GetSunLightDirection(), 0.0f);
    m_FrameConstants.mCameraPosition = Vec4(camera.GetPosition(), 1.0f);
    m_FrameConstants.mViewMatrix = camera.GetView();
    m_FrameConstants.mProjectionMatrix = final_proj_matrix;
    m_FrameConstants.mPrevViewProjectionMatrix = m_FrameConstants.mViewProjectionMatrix;
    m_FrameConstants.mViewProjectionMatrix = final_proj_matrix * camera.GetView();
    m_FrameConstants.mInvViewProjectionMatrix = glm::inverse(final_proj_matrix * camera.GetView());

    if (m_FrameCounter == 0)
    {
        m_FrameConstants.mPrevJitter = m_FrameConstants.mJitter;
        m_FrameConstants.mPrevViewProjectionMatrix = m_FrameConstants.mViewProjectionMatrix;
    }

    // TODO: instead of updating this every frame, make these buffers global?
    if (auto debug_lines_pass = m_RenderGraph.GetPass<ProbeDebugRaysData>())
    {
        m_FrameConstants.mDebugLinesVertexBuffer = inDevice.GetBindlessHeapIndex(m_RenderGraph.GetResources().GetBuffer(debug_lines_pass->GetData().mVertexBuffer));
        m_FrameConstants.mDebugLinesIndirectArgsBuffer = inDevice.GetBindlessHeapIndex(m_RenderGraph.GetResources().GetBuffer(debug_lines_pass->GetData().mIndirectArgsBuffer));
    }

    // TODO: move this somewhere else?
    if (auto gbuffer_pass = m_RenderGraph.GetPass<GBufferData>())
    {
        gbuffer_pass->GetData().mActiveEntity = inApp->GetActiveEntity();
    }

    // memcpy the frame constants into upload memory
    m_RenderGraph.GetPerFrameAllocator().AllocAndCopy(m_FrameConstants, m_RenderGraph.GetPerFrameAllocatorOffset());

    // handle PIX capture requests
    if (m_ShouldCaptureNextFrame)
    {
        PIXCaptureParameters capture_params = {};
        capture_params.GpuCaptureParameters.FileName = L"temp_pix_capture.wpix";
        gThrowIfFailed(PIXBeginCapture(PIX_CAPTURE_GPU, &capture_params));
    }

    // Start recording copy commands
    auto& copy_cmd_list = GetBackBufferData().mCopyCmdList;
    copy_cmd_list.Reset();

    //// Record pending scene uploads to the copy cmd list
    for (auto entity : m_PendingMeshUploads)
        inScene.UploadMesh(inApp, inDevice, inStagingHeap, inScene->Get<Mesh>(entity), copy_cmd_list);

    for (auto& texture_upload : m_PendingTextureUploads)
        inScene.UploadTexture(inApp, inDevice, inStagingHeap, texture_upload, copy_cmd_list);

    for (auto entity : m_PendingMaterialUploads)
        inScene.UploadMaterial(inApp, inDevice, inStagingHeap, inScene->Get<Material>(entity), copy_cmd_list);

    m_PendingMeshUploads.clear();
    m_PendingTextureUploads.clear();
    m_PendingMaterialUploads.clear();

    //// Record uploads for the RT scene 
    static const auto& upload_tlas = g_CVars.Create("upload_scene", 1, true);
    if (upload_tlas)
    {
        inScene.UploadInstances(inApp, inDevice, inStagingHeap, copy_cmd_list);
        inScene.UploadMaterials(inApp, inDevice, inStagingHeap, copy_cmd_list, m_Settings.mDisableAlbedo);
        inScene.UploadTLAS(inApp, inDevice, inStagingHeap, copy_cmd_list);
        inScene.UploadLights(inApp, inDevice, inStagingHeap, copy_cmd_list);
    }

    //// Submit all copy commands
    copy_cmd_list.Close();
    copy_cmd_list.Submit(inDevice, inDevice.GetGraphicsQueue());

    // Start recording graphics commands
    auto& direct_cmd_list = GetBackBufferData().mDirectCmdList;
    direct_cmd_list.Reset();

    // Record the entire frame into the direct cmd list
    m_RenderGraph.Execute(inDevice, direct_cmd_list, m_FrameCounter);

    //Record commands to render ImGui to the backbuffer
    if (inApp->GetSettings().mShowUI)
        RenderImGui(m_RenderGraph, inDevice, direct_cmd_list, GetBackBufferData().mBackBuffer);

    direct_cmd_list.Close();

    // Run command list execution and present in a job so it can overlap a bit with the start of the next frame
    //m_PresentJobPtr = Async::sQueueJob([&inDevice, this]() {
        // submit all graphics commands 
        direct_cmd_list.Submit(inDevice, inDevice.GetGraphicsQueue());

        auto sync_interval = m_Settings.mEnableVsync;
        auto present_flags = 0u;

        if (!m_Settings.mEnableVsync && inDevice.IsTearingSupported())
            present_flags = DXGI_PRESENT_ALLOW_TEARING;

        gThrowIfFailed(m_Swapchain->Present(sync_interval, present_flags));

        GetBackBufferData().mFenceValue++;
        m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();
        gThrowIfFailed(inDevice.GetGraphicsQueue()->Signal(m_Fence.Get(), GetBackBufferData().mFenceValue));
    //});

    m_FrameCounter++;

    if (m_ShouldCaptureNextFrame)
    {
        // Wait for the present job here to make sure we capture everything
        if (m_PresentJobPtr)
            m_PresentJobPtr->WaitCPU();

        gThrowIfFailed(PIXEndCapture(FALSE));
        m_ShouldCaptureNextFrame = false;
        ShellExecute(0, 0, "temp_pix_capture.wpix", 0, 0, SW_SHOW);
    }
}



void Renderer::Recompile(Device& inDevice, const RayTracedScene& inScene, IRenderInterface* inRenderInterface)
{
    m_RenderGraph.Clear(inDevice);


    //const auto& sky_cube_data = AddSkyCubePass(m_RenderGraph, inDevice, inScene, m_GlobalConstants.mSkyCubeTexture);

    //const auto& convolved_cube_data = AddConvolveCubePass(m_RenderGraph, inDevice, sky_cube_data.mSkyCubeTexture, m_GlobalConstants.mConvolvedSkyCubeTexture);

    const auto& default_textures = AddDefaultTexturesPass(m_RenderGraph, inDevice, inRenderInterface->GetBlackTexture(), inRenderInterface->GetWhiteTexture());
    
    auto depth_texture = default_textures.mWhiteTexture;
    auto compose_input = default_textures.mBlackTexture;

    if (m_Settings.mDoPathTrace)
    {
        compose_input = AddPathTracePass(m_RenderGraph, inDevice, inScene).mOutputTexture;
    }
    else
    {
        const auto& gbuffer_data = AddGBufferPass(m_RenderGraph, inDevice, inScene);
        
        depth_texture = gbuffer_data.mDepthTexture;

        // const auto& grass_data = AddGrassRenderPass(m_RenderGraph, inDevice, gbuffer_data);

        //const auto shadow_texture = AddShadowMaskPass(m_RenderGraph, inDevice, inScene, gbuffer_data).mOutputTexture;

        auto shadow_texture = default_textures.mWhiteTexture;

        if (m_Settings.mEnableShadows)
            shadow_texture = AddRayTracedShadowsPass(m_RenderGraph, inDevice, inScene, gbuffer_data);

        auto rtao_texture = default_textures.mWhiteTexture;

        if (m_Settings.mEnableRTAO)
            rtao_texture = AddAmbientOcclusionPass(m_RenderGraph, inDevice, inScene, gbuffer_data).mOutputTexture;

        auto reflections_texture = default_textures.mBlackTexture;

        if (m_Settings.mEnableReflections)
            reflections_texture = AddReflectionsPass(m_RenderGraph, inDevice, inScene, gbuffer_data).mOutputTexture;

        // const auto& downsample_data = AddDownsamplePass(m_RenderGraph, inDevice, reflection_data.mOutputTexture);

        auto indirect_diffuse_texture = default_textures.mBlackTexture;

        if (m_Settings.mEnableDDGI)
        {
            const auto& ddgi_trace_data = AddProbeTracePass(m_RenderGraph, inDevice, inScene);

            const auto& ddgi_update_data = AddProbeUpdatePass(m_RenderGraph, inDevice, inScene, ddgi_trace_data);
        
            const auto& ddgi_sample_data = AddProbeSamplePass(m_RenderGraph, inDevice, gbuffer_data, ddgi_update_data);

            indirect_diffuse_texture = ddgi_sample_data.mOutputTexture;
        }

        
        const auto& light_data = AddLightingPass(m_RenderGraph, inDevice, gbuffer_data, shadow_texture, reflections_texture, rtao_texture, indirect_diffuse_texture);
        
        compose_input = light_data.mOutputTexture;
        
        if (m_Settings.mEnableDDGI && m_Settings.mDebugProbes)
        {
            auto ddgi_trace_data = m_RenderGraph.GetPass<ProbeTraceData>();
            auto ddgi_update_data = m_RenderGraph.GetPass<ProbeUpdateData>();
            
            const auto& probe_debug_data = AddProbeDebugPass(m_RenderGraph, inDevice, ddgi_trace_data->GetData(), ddgi_update_data->GetData(), light_data.mOutputTexture, gbuffer_data.mDepthTexture);
        }

        if (m_Settings.mEnableDDGI && m_Settings.mDebugProbeRays)
            const auto& debug_lines_data = AddProbeDebugRaysPass(m_RenderGraph, inDevice, light_data.mOutputTexture, gbuffer_data.mDepthTexture);

        if (m_Settings.mEnableTAA)
        {
            compose_input = AddTAAResolvePass(m_RenderGraph, inDevice, gbuffer_data, light_data.mOutputTexture, m_FrameCounter).mOutputTexture;
        }
        else
        {
            switch (m_Settings.mUpscaler)
            {
                case UPSCALER_FSR:
                    compose_input = AddFsrPass(m_RenderGraph, inDevice, m_Fsr2Context, light_data.mOutputTexture, gbuffer_data).mOutputTexture;
                    break;
                case UPSCALER_DLSS:
                    compose_input = AddDLSSPass(m_RenderGraph, inDevice, m_DLSSHandle, m_DLSSParams, light_data.mOutputTexture, gbuffer_data).mOutputTexture;
                    break;
                case UPSCALER_XESS:
                    compose_input = AddXeSSPass(m_RenderGraph, inDevice, m_XeSSContext, light_data.mOutputTexture, gbuffer_data).mOutputTexture;
                    break;
            }
        }
    }

    auto bloom_output = compose_input;

    const auto debug_texture = EDebugTexture(inRenderInterface->GetSettings().mDebugTexture);
    
    // turn off any post processing effects for debug textures (this might change in the future)
    if (debug_texture == DEBUG_TEXTURE_NONE)
    {
         if (m_Settings.mEnableBloom)
            bloom_output = AddBloomPass(m_RenderGraph, inDevice, compose_input).mOutputTexture;

         // Depth of Field can't run under path tracing, no depth texture available (and we should just do actual depth of field)
        if (m_Settings.mEnableDoF && !m_Settings.mDoPathTrace)
            compose_input = AddDepthOfFieldPass(m_RenderGraph, inDevice, compose_input, depth_texture).mOutputTexture;
    }
    else
    {
        switch (debug_texture)
        { // all the debug textures that need tonemapping/gamma adjustment should go here, so they go through the compose pass
            case DEBUG_TEXTURE_RT_INDIRECT_DIFFUSE:
                if (auto data = m_RenderGraph.GetPass<ProbeSampleData>())
                    compose_input = data->GetData().mOutputTexture;
                break;

            case DEBUG_TEXTURE_RT_REFLECTIONS:
                if (auto data = m_RenderGraph.GetPass<ReflectionsData>())
                    compose_input = data->GetData().mOutputTexture;
                break;
        }

        bloom_output = compose_input;
    }

    const auto& compose_data = AddComposePass(m_RenderGraph, inDevice, bloom_output, compose_input);

    auto final_output = compose_data.mOutputTexture;

    switch (debug_texture)
    {
        case DEBUG_TEXTURE_GBUFFER_DEPTH:
        case DEBUG_TEXTURE_GBUFFER_ALBEDO:
        case DEBUG_TEXTURE_GBUFFER_NORMALS:
        case DEBUG_TEXTURE_GBUFFER_VELOCITY:
        case DEBUG_TEXTURE_GBUFFER_METALLIC:
        case DEBUG_TEXTURE_GBUFFER_ROUGHNESS:
            if (auto data = m_RenderGraph.GetPass<GBufferData>())
                final_output = AddGBufferDebugPass(m_RenderGraph, inDevice, data->GetData(), debug_texture).mOutputTexture;
            break;

        case DEBUG_TEXTURE_RT_SHADOWS:
            if (auto data = m_RenderGraph.GetPass<ClearShadowsData>())
                final_output = data->GetData().mShadowsTexture;
            break;

        case DEBUG_TEXTURE_RT_AMBIENT_OCCLUSION:
            if (auto data = m_RenderGraph.GetPass<RTAOData>())
                final_output = data->GetData().mOutputTexture;
            break;

        case DEBUG_TEXTURE_LIGHTING:
            if (auto data = m_RenderGraph.GetPass<LightingData>())
                final_output = data->GetData().mOutputTexture;
            break;
    }

    const auto& pre_imgui_data = AddPreImGuiPass(m_RenderGraph, inDevice, final_output);

    // const auto& imgui_data = AddImGuiPass(m_RenderGraph, inDevice, inStagingHeap, compose_data.mOutputTexture);

    m_RenderGraph.Compile(inDevice);
}



void Renderer::WaitForIdle(Device& inDevice)
{
    if (m_PresentJobPtr)
        m_PresentJobPtr->WaitCPU();

    for (auto& backbuffer_data : m_BackBufferData)
    {
        gThrowIfFailed(inDevice.GetGraphicsQueue()->Signal(m_Fence.Get(), backbuffer_data.mFenceValue));

        if (m_Fence->GetCompletedValue() < backbuffer_data.mFenceValue)
        {
            gThrowIfFailed(m_Fence->SetEventOnCompletion(backbuffer_data.mFenceValue, m_FenceEvent));
            WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
        }

        backbuffer_data.mFenceValue = 0;
    }

    gThrowIfFailed(m_Fence->Signal(0));
    m_FrameCounter = 0;
}



bool Renderer::InitFSR(Device& inDevice, const Viewport& inViewport)
{
    auto fsr2_desc = FfxFsr2ContextDescription
    {
        .flags = FfxFsr2InitializationFlagBits::FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE,
        .maxRenderSize = { inViewport.GetDisplaySize().x, inViewport.GetDisplaySize().y },
        .displaySize = { inViewport.GetDisplaySize().x, inViewport.GetDisplaySize().y },
        .device = ffxGetDeviceDX12(*inDevice),
    };

    m_FsrScratchMemory.resize(ffxFsr2GetScratchMemorySizeDX12());
    auto ffx_error = ffxFsr2GetInterfaceDX12(&fsr2_desc.callbacks, *inDevice, m_FsrScratchMemory.data(), m_FsrScratchMemory.size());
    if (ffx_error != FFX_OK)
        return false;

    ffx_error = ffxFsr2ContextCreate(&m_Fsr2Context, &fsr2_desc);
    if (ffx_error != FFX_OK)
        return false;

    return true;
}



bool Renderer::DestroyFSR(Device& inDevice)
{
    return ffxFsr2ContextDestroy(&m_Fsr2Context) == FFX_OK;
}



bool Renderer::InitDLSS(Device& inDevice, const Viewport& inViewport)
{
    if (!inDevice.IsDLSSSupported())
        return false;

    if (m_DLSSParams == nullptr)
        gThrowIfFailed(NVSDK_NGX_D3D12_GetCapabilityParameters(&m_DLSSParams));

    uint32_t pOutRenderOptimalWidth;
    uint32_t pOutRenderOptimalHeight;
    uint32_t pOutRenderMaxWidth;
    uint32_t pOutRenderMaxHeight;
    uint32_t pOutRenderMinWidth;
    uint32_t pOutRenderMinHeight;
    float pOutSharpnes;

    const auto upscaler_quality = Upscaler::sGetQuality(EUpscalerQuality(m_Settings.mUpscaleQuality));

    auto result = NGX_DLSS_GET_OPTIMAL_SETTINGS(m_DLSSParams, inViewport.GetDisplaySize().x, inViewport.GetDisplaySize().y,
        upscaler_quality, &pOutRenderOptimalWidth, &pOutRenderOptimalHeight, &pOutRenderMaxWidth, &pOutRenderMaxHeight, &pOutRenderMinWidth, &pOutRenderMinHeight, &pOutSharpnes);

    if (result != NVSDK_NGX_Result_Success)
        return false;

    int DlssCreateFeatureFlags = NVSDK_NGX_DLSS_Feature_Flags_None;
    DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_IsHDR;
    DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
    DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_MVJittered;
    //DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;
    //DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_DoSharpening;
    DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;

    NVSDK_NGX_DLSS_Create_Params DlssCreateParams = {};
    DlssCreateParams.Feature.InWidth = pOutRenderOptimalWidth;
    DlssCreateParams.Feature.InHeight = pOutRenderOptimalHeight;
    DlssCreateParams.Feature.InTargetWidth = inViewport.GetDisplaySize().x;
    DlssCreateParams.Feature.InTargetHeight = inViewport.GetDisplaySize().y;
    DlssCreateParams.Feature.InPerfQualityValue = upscaler_quality;
    DlssCreateParams.InFeatureCreateFlags = DlssCreateFeatureFlags;

    auto& cmd_list = StartSingleSubmit();

    result = NGX_D3D12_CREATE_DLSS_EXT(cmd_list, 1, 1, &m_DLSSHandle, m_DLSSParams, &DlssCreateParams);

    FlushSingleSubmit(inDevice, cmd_list);
    
    return result;
}



bool Renderer::DestroyDLSS(Device& inDevice)
{
    if (NVSDK_NGX_D3D12_ReleaseFeature(m_DLSSHandle) != NVSDK_NGX_Result_Success)
        return false;

    m_DLSSHandle = nullptr;

    return true;
}



bool Renderer::InitXeSS(Device& inDevice, const Viewport& inViewport)
{
    auto status = xessD3D12CreateContext(*inDevice, &m_XeSSContext);
    if (status != XESS_RESULT_SUCCESS)
        return false;

    if (XESS_RESULT_WARNING_OLD_DRIVER == xessIsOptimalDriver(m_XeSSContext))
    {
        SDL_ShowSimpleMessageBox(SDL_MessageBoxFlags::SDL_MESSAGEBOX_ERROR, "DX12 Error", "Please install the latest graphics driver from your vendor for optimal Intel(R) XeSS performance and visual quality", m_Window);
        return false;
    }

    const auto display_res = xess_2d_t { inViewport.size.x, inViewport.size.y };
    xess_properties_t props;
    status = xessGetProperties(m_XeSSContext, &display_res, &props);
    if (status != XESS_RESULT_SUCCESS)
        return false;

    xess_version_t xefx_version;
    status = xessGetIntelXeFXVersion(m_XeSSContext, &xefx_version);
    if (status != XESS_RESULT_SUCCESS)
        return false;

    const auto uav_desc_size = inDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    xess_d3d12_init_params_t params = {
        /* Output width and height. */
        display_res,
        /* Quality setting */
        XESS_QUALITY_SETTING_ULTRA_QUALITY,
        /* Initialization flags. */
        XESS_INIT_FLAG_ENABLE_AUTOEXPOSURE
    };

    status = xessD3D12Init(m_XeSSContext, &params);
    if (status != XESS_RESULT_SUCCESS)
        return false;

    // Get optimal input resolution
    auto render_res = xess_2d_t { };
    status = xessGetInputResolution(m_XeSSContext, &display_res, XESS_QUALITY_SETTING_ULTRA_QUALITY, &render_res);
    if (status != XESS_RESULT_SUCCESS)
        return false;

    return true;
}



bool Renderer::DestroyXeSS(Device& inDevice)
{
    return xessDestroyContext(m_XeSSContext) == XESS_RESULT_SUCCESS;
}



RenderInterface::RenderInterface(Application* inApp, Device& inDevice, Renderer& inRenderer, const RenderGraphResources& inResources, StagingHeap& inStagingHeap) :
    IRenderInterface(GraphicsAPI::DirectX12), m_Device(inDevice), m_Renderer(inRenderer), m_Resources(inResources), m_StagingHeap(inStagingHeap)
{
    DXGI_ADAPTER_DESC adapter_desc = {};
    m_Device.GetAdapter()->GetDesc(&adapter_desc);

    GPUInfo gpu_info = {};
    switch (adapter_desc.VendorId)
    {
        case 0x000010de: gpu_info.mVendor = "NVIDIA Corporation";           break;
        case 0x00001002: gpu_info.mVendor = "Advanced Micro Devices, Inc."; break;
        case 0x00008086: gpu_info.mVendor = "Intel Corporation";            break;
        case 0x00001414: gpu_info.mVendor = "Microsoft Corporation";        break;
    }

    gpu_info.mProduct = gWCharToString(adapter_desc.Description);
    gpu_info.mActiveAPI = "DirectX 12 Ultimate";

    SetGPUInfo(gpu_info);
}



void RenderInterface::UpdateGPUStats(Device& inDevice)
{
    m_GPUStats.mLiveBuffers.store(inDevice.GetBufferPool().GetSize());
    m_GPUStats.mLiveTextures.store(inDevice.GetTexturePool().GetSize());
    m_GPUStats.mLiveDSVHeap.store(inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV).GetSize());
    m_GPUStats.mLiveRTVHeap.store(inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV).GetSize());
    m_GPUStats.mLiveSamplerHeap.store(inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).GetSize());
    m_GPUStats.mLiveResourceHeap.store(inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetSize());
}



uint64_t RenderInterface::GetDisplayTexture()
{
    if (auto pre_imgui_pass = m_Renderer.GetRenderGraph().GetPass<PreImGuiData>())
        return m_Device.GetGPUDescriptorHandle(m_Resources.GetTextureView(pre_imgui_pass->GetData().mDisplayTextureSRV)).ptr;
    else
        return uint64_t(TextureID::INVALID);
}



uint64_t RenderInterface::GetImGuiTextureID(uint32_t inHandle)
{
    const auto& heap_index = m_Device.GetBindlessHeapIndex(TextureID(inHandle));
    const auto& resource_heap = *m_Device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const auto heap_ptr = resource_heap->GetGPUDescriptorHandleForHeapStart().ptr + heap_index * m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    return heap_ptr;
}



const char* RenderInterface::GetDebugTextureName(uint32_t inIndex) const
{
    constexpr auto names = std::array 
    {
        "None",
        "Depth",
        "Albedo",
        "Normals",
        "Velocity",
        "Metallic",
        "Roughness",
        "Lighting",
        "RT Shadows",
        "RT Reflections",
        "RT Indirect Diffuse",
        "RT Ambient Occlusion",
    };

    static_assert( names.size() == DEBUG_TEXTURE_COUNT );

    assert(inIndex < DEBUG_TEXTURE_COUNT);
    return names[inIndex];
}



void RenderInterface::UploadMeshBuffers(Entity inEntity, Mesh& inMesh)
{
    const auto& vertices = inMesh.GetInterleavedVertices();
    const auto  vertices_size = vertices.size() * sizeof(vertices[0]);
    const auto  indices_size = inMesh.indices.size() * sizeof(inMesh.indices[0]);

    if (!vertices_size || !indices_size)
        return;

    inMesh.indexBuffer = m_Device.CreateBuffer(Buffer::Desc{
        .size   = uint32_t(indices_size),
        .stride = sizeof(uint32_t) * 3,
        .usage  = Buffer::Usage::INDEX_BUFFER,
        .debugName = "INDEX_BUFFER"
    }).ToIndex();

    inMesh.vertexBuffer = m_Device.CreateBuffer(Buffer::Desc{
        .size   = uint32_t(vertices_size),
        .stride = sizeof(Vertex),
        .usage  = Buffer::Usage::VERTEX_BUFFER,
        .debugName = "VERTEX_BUFFER"
    }).ToIndex();

    // actual data upload happens in RayTracedScene::UploadMesh at the start of the frame
    m_Renderer.QueueMeshUpload(inEntity);
}



void RenderInterface::UploadMaterialTextures(Entity inEntity, Material& inMaterial, Assets& inAssets)
{
    IRenderInterface::UploadMaterialTextures(inEntity, inMaterial, inAssets);
    // UploadMaterialTextures only creates the gpu texture and srv, data upload happens in RayTracedScene::UploadMaterial at the start of the frame
    m_Renderer.QueueMaterialUpload(inEntity);
}



uint32_t RenderInterface::UploadTextureFromAsset(const TextureAsset::Ptr& inAsset, bool inIsSRGB, uint8_t inSwizzle)
{
    auto data_ptr = inAsset->GetData();
    const auto header_ptr = inAsset->GetHeader();

    const auto mipmap_levels = header_ptr->dwMipMapCount;
    const auto debug_name = inAsset->GetPath().string();

    auto format = inIsSRGB ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM;
    auto dds_format = (EDDSFormat)header_ptr->ddspf.dwFourCC;

    if (dds_format == EDDSFormat::DDS_FORMAT_DX10)
        format = (DXGI_FORMAT)inAsset->GetHeaderDXT10()->dxgiFormat;

    const auto texture_id = m_Device.CreateTexture(Texture::Desc
    {
        .swizzle    = inSwizzle,
        .format     = format,
        .width      = header_ptr->dwWidth,
        .height     = header_ptr->dwHeight,
        .mipLevels  = mipmap_levels,
        .usage      = Texture::SHADER_READ_ONLY,
        .debugName  = debug_name.c_str()
    });

    return uint32_t(texture_id);
}



void RenderInterface::OnResize(const Viewport& inViewport)
{
    m_Renderer.SetShouldResize(true);
}



CommandList& Renderer::StartSingleSubmit()
{
    auto& cmd_list = GetBackBufferData().mDirectCmdList;
    cmd_list.Begin();
    return cmd_list;
}



void Renderer::FlushSingleSubmit(Device& inDevice, CommandList& inCmdList)
{
    inCmdList.Close();

    const auto cmd_lists = std::array { (ID3D12CommandList*)inCmdList };
    inDevice.GetGraphicsQueue()->ExecuteCommandLists(cmd_lists.size(), cmd_lists.data());

    auto& backbuffer_data = GetBackBufferData();
    backbuffer_data.mFenceValue++;
    gThrowIfFailed(inDevice.GetGraphicsQueue()->Signal(m_Fence.Get(), backbuffer_data.mFenceValue));
    gThrowIfFailed(m_Fence->SetEventOnCompletion(backbuffer_data.mFenceValue, m_FenceEvent));
    WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
}



void RenderInterface::DrawDebugSettings(Application* inApp, Scene& inScene, const Viewport& inViewport)
{
    bool need_recompile = false;

    ImGui::SeparatorText("Renderer Settings");

    need_recompile |= ImGui::Checkbox("##pathtracingtoggle", (bool*)&m_Renderer.GetSettings().mDoPathTrace);

    ImGui::SameLine();

    ImGui::AlignTextToFramePadding();

    if (ImGui::BeginMenu("Enable Path Tracer"))
    {
        ImGui::SeparatorText("Settings");

        if (ImGui::SliderInt("Bounces", (int*)&PathTraceData::mBounces, 1, 8))
            PathTraceData::mReset = true;

        ImGui::EndMenu();
    }

    need_recompile |= ImGui::Checkbox("##TAAtoggle", (bool*)&m_Renderer.GetSettings().mEnableTAA);
    
    ImGui::SameLine();

    ImGui::AlignTextToFramePadding();

    if (ImGui::BeginMenu("Enable Temporal AA"))
    {
        ImGui::SeparatorText("Upscaler");

        if (m_Renderer.GetSettings().mEnableTAA)
        {
            constexpr auto text = "...TAA is enabled";
            ImGui::SetCursorPosX(( ImGui::GetWindowWidth() - ImGui::CalcTextSize(text).x ) / 2.f);
            ImGui::TextDisabled(text);
        }
        else
        {
            constexpr auto upscaler_items = std::array { "No Upscaler", "AMD FSR2", "Nvidia DLSS", "Intel XeSS" };
            constexpr auto upscaler_quality_items = std::array { "Native", "Quality", "Balanced", "Performance" };

            if (ImGui::BeginCombo("##Upscaler", upscaler_items[m_Renderer.GetSettings().mUpscaler], ImGuiComboFlags_None))
            {
                for (uint32_t upscaler = 0; upscaler < UPSCALER_COUNT; upscaler++)
                {
                    if (ImGui::Selectable(upscaler_items[upscaler], m_Renderer.GetSettings().mUpscaler == upscaler))
                    {
                        m_Renderer.GetSettings().mUpscaler = upscaler;
                        need_recompile = true;
                    }
                }

                ImGui::EndCombo();
            }

            if (m_Renderer.GetSettings().mUpscaler != UPSCALER_NONE)
            {
                if (ImGui::BeginCombo("##UpscalerQuality", upscaler_quality_items[m_Renderer.GetSettings().mUpscaleQuality], ImGuiComboFlags_None))
                {
                    for (uint32_t quality = 0; quality < UPSCALER_QUALITY_COUNT; quality++)
                    {
                        if (ImGui::Selectable(upscaler_quality_items[quality], m_Renderer.GetSettings().mUpscaleQuality == quality))
                        {
                            m_Renderer.GetSettings().mUpscaleQuality = quality;
                            need_recompile = true;
                        }
                    }

                    ImGui::EndCombo();
                }
            }
        }

        ImGui::EndMenu();
    }

    ImGui::AlignTextToFramePadding();

    if (ImGui::BeginMenu("Debug Settings"))
    {
        ImGui::SeparatorText("Debug");

        if (ImGui::Button("PIX GPU Capture"))
            m_Renderer.SetShouldCaptureNextFrame(true);

        if (ImGui::Button("Generate Meshlets"))
        {
            Timer timer;

            inApp->LogMessage("Generating Meshlets..");

            for (const auto& [entity, mesh] : inScene.Each<Mesh>())
            {
                const size_t max_vertices = 64;
                const size_t max_triangles = 124;
                const float cone_weight = 0.0f;

                size_t max_meshlets = meshopt_buildMeshletsBound(mesh.indices.size(), max_vertices, max_triangles);
                std::vector<meshopt_Meshlet> meshlets(max_meshlets);
                std::vector<unsigned int> meshlet_vertices(max_meshlets * max_vertices);
                std::vector<unsigned char> meshlet_triangles(max_meshlets * max_triangles * 3);

                size_t meshlet_count = meshopt_buildMeshlets(meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(), mesh.indices.data(),
                    mesh.indices.size(), &mesh.positions[0].x, mesh.positions.size(), sizeof(mesh.positions[0]), max_vertices, max_triangles, cone_weight);

                meshlets.resize(meshlet_count);
                meshlet_vertices.resize(meshlet_count * max_vertices);
                meshlet_triangles.resize(meshlet_count * max_triangles * 3);

                mesh.meshlets.reserve(meshlet_count);
                mesh.meshletIndices.resize(meshlet_count * max_vertices);
                mesh.meshletTriangles.resize(meshlet_count * max_triangles);

                for (const auto& meshlet : meshlets)
                {
                    auto& m = mesh.meshlets.emplace_back();
                    m.mTriangleCount = meshlet.triangle_count;
                    m.mTriangleOffset = meshlet.triangle_offset;
                    m.mVertexCount = meshlet.vertex_count;
                    m.mVertexOffset = meshlet.vertex_offset;
                }

                assert(mesh.meshletIndices.size() == meshlet_vertices.size());
                std::memcpy(mesh.meshletIndices.data(), meshlet_vertices.data(), mesh.meshletIndices.size());

                for (uint32_t idx = 0; idx < mesh.meshletTriangles.size(); idx += 3)
                {
                    mesh.meshletTriangles[idx].mX = meshlet_triangles[idx];
                    mesh.meshletTriangles[idx].mY = meshlet_triangles[idx + 1];
                    mesh.meshletTriangles[idx].mZ = meshlet_triangles[idx + 2];
                }
            }

            inApp->LogMessage(std::format("Generating Meshlets took {:.3f} seconds.", timer.GetElapsedTime()));
        }


        if (ImGui::Button("Save As GraphViz.."))
        {
            const auto file_path = OS::sSaveFileDialog("DOT File (*.dot)\0", "dot");

            if (!file_path.empty())
            {
                auto ofs = std::ofstream(file_path);
                ofs << m_Renderer.GetRenderGraph().ToGraphVizText(m_Device, TextureID());
            }
        }

        ImGui::EndMenu();
    }

    ImGui::AlignTextToFramePadding();


    if (ImGui::BeginMenu("Monitor Settings"))
    {
        ImGui::SeparatorText("Monitor");

        static constexpr auto modes = std::array { 0u /* Windowed */, (uint32_t)SDL_WINDOW_FULLSCREEN_DESKTOP, (uint32_t)SDL_WINDOW_FULLSCREEN };
        static constexpr auto mode_strings = std::array { "Windowed", "Borderless", "Fullscreen" };

        auto mode = 0u; // Windowed
        auto window = inApp->GetWindow();
        const auto window_flags = SDL_GetWindowFlags(window);

        if (SDL_IsWindowBorderless(window))
            mode = 1u;
        else if (SDL_IsWindowExclusiveFullscreen(window))
            mode = 2u;

        if (ImGui::BeginCombo("Mode", mode_strings[mode]))
        {
            for (const auto& [index, string] : gEnumerate(mode_strings))
            {
                const auto is_selected = index == mode;
                if (ImGui::Selectable(string, &is_selected))
                {
                    bool to_fullscreen = modes[index] == 2u;
                    SDL_SetWindowFullscreen(window, modes[index]);
                    if (to_fullscreen)
                    {
                        while (!SDL_IsWindowExclusiveFullscreen(window))
                            SDL_Delay(10);
                    }

                    m_Renderer.SetShouldResize(true);
                }
            }

            ImGui::EndCombo();
        }

        auto display_names = std::vector<std::string>(SDL_GetNumVideoDisplays());
        for (const auto& [index, name] : gEnumerate(display_names))
            name = SDL_GetDisplayName(index);

        const auto display_index = SDL_GetWindowDisplayIndex(window);
        if (ImGui::BeginCombo("Monitor", display_names[display_index].c_str()))
        {
            for (const auto& [index, name] : gEnumerate(display_names))
            {
                const auto is_selected = index == display_index;
                if (ImGui::Selectable(name.c_str(), &is_selected))
                {
                    if (m_Renderer.GetSettings().mFullscreen)
                    {
                        SDL_SetWindowFullscreen(window, 0);
                        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED_DISPLAY(index), SDL_WINDOWPOS_CENTERED_DISPLAY(index));
                        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                    }
                    else
                    {
                        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED_DISPLAY(index), SDL_WINDOWPOS_CENTERED_DISPLAY(index));
                    }

                    m_Renderer.SetShouldResize(true);
                }
            }

            ImGui::EndCombo();
        }

        const auto current_display_index = SDL_GetWindowDisplayIndex(window);
        const auto num_display_modes = SDL_GetNumDisplayModes(current_display_index);

        auto display_modes = std::vector<SDL_DisplayMode>(num_display_modes);
        auto display_mode_strings = std::vector<std::string>(num_display_modes);

        SDL_DisplayMode* new_display_mode = nullptr;

        if (m_Renderer.GetSettings().mFullscreen)
        {
            for (const auto& [index, display_mode] : gEnumerate(display_modes))
                SDL_GetDisplayMode(current_display_index, index, &display_mode);

            for (const auto& [index, display_mode] : gEnumerate(display_modes))
                display_mode_strings[index] = std::format("{}x{}@{}Hz", display_mode.w, display_mode.h, display_mode.refresh_rate);

            SDL_DisplayMode current_display_mode;
            SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &current_display_mode);

            if (ImGui::BeginCombo("Resolution", display_mode_strings[m_Renderer.GetSettings().mDisplayRes].c_str()))
            {
                for (int index = 0; index < display_mode_strings.size(); index++)
                {
                    const auto is_selected = index == m_Renderer.GetSettings().mDisplayRes;
                    if (ImGui::Selectable(display_mode_strings[index].c_str(), is_selected))
                    {
                        m_Renderer.GetSettings().mDisplayRes = index;
                        new_display_mode = &display_modes[index];

                        if (SDL_SetWindowDisplayMode(window, new_display_mode) != 0)
                            std::cout << SDL_GetError();

                        std::cout << std::format("SDL_SetWindowDisplayMode with {}x{} @ {}Hz\n", new_display_mode->w, new_display_mode->h, new_display_mode->refresh_rate);

                        m_Renderer.SetShouldResize(true);
                    }
                }

                ImGui::EndCombo();
            }
        }

        ImGui::SliderInt("V-Sync", &m_Renderer.GetSettings().mEnableVsync, 0, 3);


        ImGui::EndMenu();
    }

    /*if (auto grass_pass = m_Renderer.GetRenderGraph().GetPass<GrassData>())
    {
        ImGui::Text("Grass Settings");
        ImGui::DragFloat("Grass Bend", &grass_pass->GetData().mRenderConstants.mBend, 0.01f, -1.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Grass Tilt", &grass_pass->GetData().mRenderConstants.mTilt, 0.01f, -1.0f, 1.0f, "%.2f");
        ImGui::DragFloat2("Wind Direction", glm::value_ptr(grass_pass->GetData().mRenderConstants.mWindDirection), 0.01f, -10.0f, 10.0f, "%.1f");
        ImGui::NewLine();
    }*/

    ImGui::AlignTextToFramePadding();

    if (ImGui::BeginMenu("Lighting Settings"))
    {
        //ImGui::SeparatorText("Settings");
        ImGui::SeparatorText("Lighting");

        need_recompile |= ImGui::Checkbox("Enable Shadows", (bool*)&m_Renderer.GetSettings().mEnableShadows);
        need_recompile |= ImGui::Checkbox("Enable Reflections", (bool*)&m_Renderer.GetSettings().mEnableReflections);

        need_recompile |= ImGui::Checkbox("##GItoggle", (bool*)&m_Renderer.GetSettings().mEnableDDGI);
        ImGui::SameLine();

        if (ImGui::BeginMenu("Enable Indirect Diffuse"))
        {
            ImGui::SeparatorText("Debug Options");

            ImGui::Checkbox("Visualize Pure White Mode", (bool*)&m_Renderer.GetSettings().mDisableAlbedo);
            need_recompile |= ImGui::Checkbox("Visualize Indirect Diffuse Rays", (bool*)&m_Renderer.GetSettings().mDebugProbeRays);
            need_recompile |= ImGui::Checkbox("Visualize Indirect Diffuse Probes", (bool*)&m_Renderer.GetSettings().mDebugProbes);
            ImGui::EndMenu();
        }

        need_recompile |= ImGui::Checkbox("##AOtoggle", (bool*)&m_Renderer.GetSettings().mEnableRTAO);
        ImGui::SameLine();

        if (ImGui::BeginMenu("Enable Ambient Occlusion"))
        {
            ImGui::SeparatorText("Settings");

            ImGui::DragFloat("Radius", &RTAOData::mParams.mRadius, 0.01f, 0.0f, 20.0f, "%.2f");
            ImGui::DragFloat("Intensity", &RTAOData::mParams.mPower, 0.01f, 0.0f, 10.0f, "%.2f");
            ImGui::DragFloat("Normal Bias", &RTAOData::mParams.mNormalBias, 0.001f, 0.0f, 1.0f, "%.3f");
            ImGui::SliderInt("Sample Count", (int*)&RTAOData::mParams.mSampleCount, 1u, 128u);
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    ImGui::AlignTextToFramePadding();

    if (ImGui::BeginMenu("Post Process Settings"))
    {
        ImGui::SeparatorText("Post Processing");

        need_recompile |= ImGui::Checkbox("##Bloomtoggle", (bool*)&m_Renderer.GetSettings().mEnableBloom);
        ImGui::SameLine();

        if (ImGui::BeginMenu("Bloom"))
        {
            ImGui::SeparatorText("Settings");

            ImGui::DragFloat("Blend Factor", &ComposeData::mBloomBlendFactor, 0.01f, 0.0f, 0.5f, "%.2f");

            ImGui::EndMenu();
        }


        need_recompile |= ImGui::Checkbox("##DOFtoggle", (bool*)&m_Renderer.GetSettings().mEnableDoF);
        ImGui::SameLine();


        if (ImGui::BeginMenu("Depth of Field"))
        {
            ImGui::SeparatorText("Settings");

            const auto far_plane = inViewport.GetCamera().GetFar();
            const auto near_plane = inViewport.GetCamera().GetNear();

            ImGui::DragFloat("Focus Scale", &DepthOfFieldData::mFocusScale, 0.01f, 0.0f, 4.0f, "%.2f");
            ImGui::DragFloat("Focus Point", &DepthOfFieldData::mFocusPoint, 0.01f, near_plane, far_plane, "%.2f");
            ImGui::EndMenu();
        }

        need_recompile |= ImGui::Checkbox("Auto Exposure", (bool*)&m_Renderer.GetSettings().mEnableAutoExposure);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Currently not implemented.");

        ImGui::DragFloat("Manual Exposure", &ComposeData::mExposure, 0.01f, 0.0f, 100.0f, "%.2f");
        ImGui::DragFloat("Chromatic Aberration", &ComposeData::mChromaticAberrationStrength, 0.01f, 0.0f, 10.0f, "%.2f");

        ImGui::EndMenu();
    }

    if (need_recompile)
        m_Renderer.SetShouldResize(true); // call for a resize so the rendergraph gets recompiled (hacky, TODO: FIXME: pls fix)
}



TextureID InitImGui(Device& inDevice, DXGI_FORMAT inRtvFormat, uint32_t inFrameCount)
{
    auto width = 0, height = 0;
    auto pixels = static_cast<unsigned char*>( nullptr );
    ImGui::GetIO().Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

    auto font_texture_id = inDevice.CreateTexture(Texture::Desc
    {
        .format = DXGI_FORMAT_R8_UNORM,
        .width  = uint32_t(width),
        .height = uint32_t(height),
        .usage  = Texture::SHADER_READ_ONLY,
        .debugName = "IMGUI_FONT_TEXTURE"
    });

    auto font_texture_view = inDevice.GetTexture(font_texture_id).GetView();
    auto& descriptor_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    ImGui_ImplDX12_Init(
        *inDevice,
        inFrameCount,
        inRtvFormat,
        *descriptor_heap,
        descriptor_heap.GetCPUDescriptorHandle(font_texture_view),
        descriptor_heap.GetGPUDescriptorHandle(font_texture_view)
    );

    ImGui_ImplDX12_CreateDeviceObjects();

    //auto imgui_id = (void*)(intptr_t)inDevice.GetBindlessHeapIndex(font_texture_id);
    //ImGui::GetIO().Fonts->SetTexID(imgui_id);

    return font_texture_id;
}



void RenderImGui(RenderGraph& inRenderGraph, Device& inDevice, CommandList& inCmdList, TextureID inBackBuffer)
{
    PIXScopedEvent(static_cast<ID3D12GraphicsCommandList*>( inCmdList ), PIX_COLOR(0, 255, 0), "IMGUI BACKEND PASS");

    // Just in-case we did some external pass like FSR2 before this that sets its own descriptor heaps
    inDevice.BindDrawDefaults(inCmdList);

    {   // manual barriers around the imported backbuffer resource, the rendergraph doesn't handle this kind of state
        auto backbuffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetD3D12Resource(inBackBuffer), D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
        inCmdList->ResourceBarrier(1, &backbuffer_barrier);
    }

    const auto bb_viewport = CD3DX12_VIEWPORT(inDevice.GetTexture(inBackBuffer).GetD3D12Resource().Get());
    const auto bb_scissor = CD3DX12_RECT(bb_viewport.TopLeftX, bb_viewport.TopLeftY, bb_viewport.Width, bb_viewport.Height);

    inCmdList->RSSetViewports(1, &bb_viewport);
    inCmdList->RSSetScissorRects(1, &bb_scissor);

    const auto& rtv_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    const auto rtv = std::array { rtv_heap.GetCPUDescriptorHandle(inDevice.GetTexture(inBackBuffer).GetView()) };

    inCmdList->OMSetRenderTargets(rtv.size(), rtv.data(), FALSE, nullptr);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), inCmdList);

    {
        auto backbuffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetD3D12Resource(inBackBuffer), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON);
        inCmdList->ResourceBarrier(1, &backbuffer_barrier);
    }
}


} // raekor
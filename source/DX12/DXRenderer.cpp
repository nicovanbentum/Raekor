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

RTTI_DEFINE_TYPE(DDGISceneSettings) {}

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

        backbuffer_data.mDirectCmdList = CommandList(inDevice, index);
        backbuffer_data.mDirectCmdList->SetName(L"Raekor::DX12::CommandList(DIRECT)");

        backbuffer_data.mCopyCmdList = CommandList(inDevice, index);
        backbuffer_data.mCopyCmdList->SetName(L"Raekor::DX12::CommandList(COPY)");

        rtv_resource->SetName(L"BACKBUFFER");
    }

    inDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));
    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (!m_FenceEvent)
        gThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));

    /// INIT ALL THE UPSCALERS ///

    //if (!InitFSR(inDevice, inViewport))
    //    std::cout << std::format("Failed to initialize FSR2.\n");

    //if (!InitXeSS(inDevice, inViewport))
    //    std::cout << std::format("Failed to initialize XeSS.\n");

    //if (inDevice.IsDLSSSupported())
    //{
    //    if (!InitDLSS(inDevice, inViewport))
    //        std::cout << std::format("Failed to initialize DLSS.\n");
    //}
}



void Renderer::OnResize(Device& inDevice, const Viewport& inViewport, bool inFullScreen)
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

    /*DestroyFSR(inDevice);
    if (!InitFSR(inDevice, inViewport))
        std::cout << std::format("Failed to initialize FSR2.\n");

    DestroyXeSS(inDevice);
    if (!InitXeSS(inDevice, inViewport))
        std::cout << std::format("Failed to initialize XeSS.\n");

    if (inDevice.IsDLSSSupported())
    {
        DestroyDLSS(inDevice);
        InitDLSS(inDevice, inViewport);
    }*/

    m_Settings.mFullscreen = inFullScreen;
}



void Renderer::OnRender(Application* inApp, Device& inDevice, const Viewport& inViewport, RayTracedScene& inScene, StagingHeap& inStagingHeap, IRenderInterface* inRenderInterface, float inDeltaTime)
{
    // Check if any of the shader sources were updated and recompile them if necessary
    bool need_recompile = g_SystemShaders.HotLoad();
    if (need_recompile)
        std::cout << std::format("Hotloaded system shaders.\n");

    static auto do_stress_test = OS::sCheckCommandLineOption("-stress_test");

    if (m_ShouldResize || need_recompile || (do_stress_test && m_FrameCounter > 0))
    {
        // Make sure nothing is using render targets anymore
        WaitForIdle(inDevice);

        // Resize the renderer, which recreates the swapchain backbuffers and re-inits upscalers
        OnResize(inDevice, inViewport, SDL_IsWindowExclusiveFullscreen(m_Window));

        // Recompile the renderer, super overkill and leaks memory. TODO: pls fix
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
    inStagingHeap.RetireBuffers(backbuffer_data.mCopyCmdList);
    inStagingHeap.RetireBuffers(backbuffer_data.mDirectCmdList);
    backbuffer_data.mCopyCmdList.ReleaseTrackedResources();
    backbuffer_data.mDirectCmdList.ReleaseTrackedResources();

    // Update the total running time of the application / renderer
    m_ElapsedTime += inDeltaTime;

    // Calculate a jittered projection matrix for TAA/FSR/DLSS/XESS
    const auto& camera = m_RenderGraph.GetViewport().GetCamera();
    const auto jitter_phase_count = ffxFsr2GetJitterPhaseCount(m_RenderGraph.GetViewport().size.x, m_RenderGraph.GetViewport().size.x);

    float jitter_offset_x = 0;
    float jitter_offset_y = 0;
    ffxFsr2GetJitterOffset(&jitter_offset_x, &jitter_offset_y, m_FrameCounter, jitter_phase_count);

    const auto jitter_x = 2.0f * jitter_offset_x / (float)m_RenderGraph.GetViewport().size.x;
    const auto jitter_y = -2.0f * jitter_offset_y / (float)m_RenderGraph.GetViewport().size.y;
    const auto jitter_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(jitter_x, jitter_y, 0));
    //const auto jittered_proj_matrix = camera.GetProjection();
    auto jittered_proj_matrix = m_Settings.mEnableTAA ? jitter_matrix * camera.GetProjection() : camera.GetProjection();

    // Update all the frame constants and copy it in into the GPU ring buffer
    m_FrameConstants.mTime = m_ElapsedTime;
    m_FrameConstants.mDeltaTime = inDeltaTime;
    m_FrameConstants.mFrameIndex = m_FrameCounter % sFrameCount;
    m_FrameConstants.mFrameCounter = m_FrameCounter;
    m_FrameConstants.mPrevJitter = m_FrameConstants.mJitter;
    m_FrameConstants.mJitter = Vec2(jitter_x, jitter_y);
    m_FrameConstants.mSunColor = inScene->GetSunLight() ? inScene->GetSunLight()->GetColor() : Vec4(1.0f);
    m_FrameConstants.mSunDirection = Vec4(inScene->GetSunLightDirection(), 0.0f);
    m_FrameConstants.mCameraPosition = Vec4(camera.GetPosition(), 1.0f);
    m_FrameConstants.mViewMatrix = camera.GetView();
    m_FrameConstants.mProjectionMatrix = jittered_proj_matrix;
    m_FrameConstants.mPrevViewProjectionMatrix = m_FrameConstants.mViewProjectionMatrix;
    m_FrameConstants.mViewProjectionMatrix = jittered_proj_matrix * camera.GetView();
    m_FrameConstants.mInvViewProjectionMatrix = glm::inverse(jittered_proj_matrix * camera.GetView());

    if (m_FrameCounter == 0)
    {
        m_FrameConstants.mPrevJitter = m_FrameConstants.mJitter;
        m_FrameConstants.mPrevViewProjectionMatrix = m_FrameConstants.mViewProjectionMatrix;
    }

    // TODO: instead of updating this every frame, make these buffers global?
    if (auto debug_lines_pass = m_RenderGraph.GetPass<DebugLinesData>())
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

    //// Record copy commands to the copy cmd list
    // TODO: StagingBuffer leaks and ReleaseBuffer/ReleaseTexture needs frame tracking
    inScene.UploadInstances(inApp, inDevice, inStagingHeap, copy_cmd_list);
    inScene.UploadMaterials(inApp, inDevice, inStagingHeap, copy_cmd_list);
    inScene.UploadTLAS(inApp, inDevice, inStagingHeap, copy_cmd_list);

    //// Submit all copy commands
    copy_cmd_list.Close();
    copy_cmd_list.Submit(inDevice, inDevice.GetGraphicsQueue());

    // Start recording graphics commands
    auto& direct_cmd_list = GetBackBufferData().mDirectCmdList;
    direct_cmd_list.Reset();

    // Record the entire frame into the direct cmd list
    m_RenderGraph.Execute(inDevice, direct_cmd_list, m_FrameCounter);

    //Record commands to render ImGui to the backbuffer
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

    const auto& gbuffer_data = AddGBufferPass(m_RenderGraph, inDevice, inScene);
    
    auto compose_input = gbuffer_data.mRenderTexture;
    
    if (m_Settings.mDoPathTrace)
    {
        compose_input = AddPathTracePass(m_RenderGraph, inDevice, inScene).mOutputTexture;
    }
    else
    {
        // const auto& grass_data = AddGrassRenderPass(m_RenderGraph, inDevice, gbuffer_data);
        
        const auto& shadow_data = AddShadowMaskPass(m_RenderGraph, inDevice, inScene, gbuffer_data);
    
        const auto& rtao_data = AddAmbientOcclusionPass(m_RenderGraph, inDevice, inScene, gbuffer_data);
        
        const auto& reflection_data = AddReflectionsPass(m_RenderGraph, inDevice, inScene, gbuffer_data);
        
        // const auto& downsample_data = AddDownsamplePass(m_RenderGraph, inDevice, reflection_data.mOutputTexture);
        
        const auto& ddgi_trace_data = AddProbeTracePass(m_RenderGraph, inDevice, inScene);

        const auto& ddgi_update_data = AddProbeUpdatePass(m_RenderGraph, inDevice, ddgi_trace_data);
        
        const auto& light_data = AddLightingPass(m_RenderGraph, inDevice, gbuffer_data, shadow_data, reflection_data, rtao_data.mOutputTexture, ddgi_update_data);
        
        compose_input = light_data.mOutputTexture;
        
        if (m_Settings.mProbeDebug)
            const auto& probe_debug_data = AddProbeDebugPass(m_RenderGraph, inDevice, ddgi_trace_data, ddgi_update_data, light_data.mOutputTexture, gbuffer_data.mDepthTexture);

        if (m_Settings.mDebugLines)
            const auto& debug_lines_data = AddDebugLinesPass(m_RenderGraph, inDevice, light_data.mOutputTexture, gbuffer_data.mDepthTexture);
        
        if (m_Settings.mEnableTAA)
        {
            compose_input = AddTAAResolvePass(m_RenderGraph, inDevice, gbuffer_data, light_data.mOutputTexture, m_FrameCounter).mOutputTexture;
        }
        else
        {
            switch (m_Settings.mUpscaler)
            {
                case UPSCALER_FSR2:
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

    const auto& compose_data = AddComposePass(m_RenderGraph, inDevice, compose_input);

    auto final_output = compose_data.mOutputTexture;

    const auto debug_texture = EDebugTexture(inRenderInterface->GetSettings().mDebugTexture);

    switch (debug_texture)
    {
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
            final_output = gbuffer_data.mVelocityTexture;
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
        .maxRenderSize = { inViewport.size.x, inViewport.size.y },
        .displaySize = { inViewport.size.x, inViewport.size.y },
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
    if (m_DLSSParams == nullptr)
        gThrowIfFailed(NVSDK_NGX_D3D12_GetCapabilityParameters(&m_DLSSParams));

    uint32_t pOutRenderOptimalWidth;
    uint32_t pOutRenderOptimalHeight;
    uint32_t pOutRenderMaxWidth;
    uint32_t pOutRenderMaxHeight;
    uint32_t pOutRenderMinWidth;
    uint32_t pOutRenderMinHeight;
    float pOutSharpnes;

    auto result = NGX_DLSS_GET_OPTIMAL_SETTINGS(m_DLSSParams, inViewport.size.x, inViewport.size.y,
        NVSDK_NGX_PerfQuality_Value_DLAA, &pOutRenderOptimalWidth, &pOutRenderOptimalHeight, &pOutRenderMaxWidth, &pOutRenderMaxHeight, &pOutRenderMinWidth, &pOutRenderMinHeight, &pOutSharpnes);

    if (result != NVSDK_NGX_Result_Success)
        return false;

    int MotionVectorResolutionLow = 1;
    // Next create features	
    int DlssCreateFeatureFlags = NVSDK_NGX_DLSS_Feature_Flags_None;
    DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
    DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_IsHDR;
    DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_DoSharpening;
    DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;

    NVSDK_NGX_DLSS_Create_Params DlssCreateParams = {};
    DlssCreateParams.Feature.InWidth = pOutRenderOptimalWidth;
    DlssCreateParams.Feature.InHeight = pOutRenderOptimalHeight;
    DlssCreateParams.Feature.InTargetWidth = inViewport.size.x;
    DlssCreateParams.Feature.InTargetHeight = inViewport.size.y;
    DlssCreateParams.Feature.InPerfQualityValue = NVSDK_NGX_PerfQuality_Value_DLAA;
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



RenderInterface::RenderInterface(Device& inDevice, Renderer& inRenderer, const RenderGraphResources& inResources, StagingHeap& inStagingHeap) :
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
    const auto& resource_heap = *m_Device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const auto heap_ptr = resource_heap->GetGPUDescriptorHandleForHeapStart().ptr + inHandle * m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    return heap_ptr;
}



const char* RenderInterface::GetDebugTextureName(uint32_t inIndex) const
{
    constexpr auto names = std::array 
    {
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

    static_assert( names.size() == DEBUG_TEXTURE_COUNT );

    assert(inIndex < DEBUG_TEXTURE_COUNT);
    return names[inIndex];
}



void RenderInterface::UploadMeshBuffers(Mesh& inMesh)
{
    m_Renderer.WaitForIdle(m_Device);

    const auto vertices = inMesh.GetInterleavedVertices();
    const auto vertices_size = vertices.size() * sizeof(vertices[0]);
    const auto indices_size = inMesh.indices.size() * sizeof(inMesh.indices[0]);

    if (!vertices_size || !indices_size)
        return;

    inMesh.indexBuffer = m_Device.CreateBuffer(Buffer::Desc{
        .size   = uint32_t(indices_size),
        .stride = sizeof(uint32_t) * 3,
        .usage  = Buffer::Usage::INDEX_BUFFER,
        .debugName = L"INDEX_BUFFER"
    }).ToIndex();

    inMesh.vertexBuffer = m_Device.CreateBuffer(Buffer::Desc{
        .size   = uint32_t(vertices_size),
        .stride = sizeof(Vertex),
        .usage  = Buffer::Usage::VERTEX_BUFFER,
        .debugName = L"VERTEX_BUFFER"
    }).ToIndex();

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

    const auto blas_buffer_id = m_Device.CreateBuffer(Buffer::Desc 
    {
        .size  = prebuild_info.ResultDataMaxSizeInBytes,
        .usage = Buffer::Usage::ACCELERATION_STRUCTURE,
        .debugName = L"BLAS_BUFFER"
    });

    inMesh.BottomLevelAS = blas_buffer_id.ToIndex();

    const auto scratch_buffer_id = m_Device.CreateBuffer(Buffer::Desc
    {
        .size = prebuild_info.ScratchDataSizeInBytes,
        .debugName = L"SCRATCH_BUFFER_BLAS_RT"
    });

    auto& blas_buffer = m_Device.GetBuffer(blas_buffer_id);
    auto& scratch_buffer = m_Device.GetBuffer(scratch_buffer_id);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
    desc.ScratchAccelerationStructureData = scratch_buffer->GetGPUVirtualAddress();
    desc.DestAccelerationStructureData = blas_buffer->GetGPUVirtualAddress();
    desc.Inputs = inputs;

    auto& cmd_list = m_Renderer.StartSingleSubmit();

    {
        const auto& gpu_index_buffer = m_Device.GetBuffer(BufferID(inMesh.indexBuffer));
        const auto& gpu_vertex_buffer = m_Device.GetBuffer(BufferID(inMesh.vertexBuffer));

        m_StagingHeap.StageBuffer(cmd_list, gpu_index_buffer.GetD3D12Resource(), 0, inMesh.indices.data(), indices_size);
        m_StagingHeap.StageBuffer(cmd_list, gpu_vertex_buffer.GetD3D12Resource(), 0, vertices.data(), vertices_size);
    }

    {
        const auto& gpu_index_buffer = m_Device.GetBuffer(BufferID(inMesh.indexBuffer));
        const auto& gpu_vertex_buffer = m_Device.GetBuffer(BufferID(inMesh.vertexBuffer));

        const auto barriers = std::array {
            CD3DX12_RESOURCE_BARRIER::Transition(gpu_index_buffer.GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ),
            CD3DX12_RESOURCE_BARRIER::Transition(gpu_vertex_buffer.GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ)
        };

        cmd_list->ResourceBarrier(barriers.size(), barriers.data());
    }

    cmd_list->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);

    m_Renderer.FlushSingleSubmit(m_Device, cmd_list);

    // m_Device.ReleaseBuffer(scratch_buffer_id);
}



uint32_t RenderInterface::UploadTextureFromAsset(const TextureAsset::Ptr& inAsset, bool inIsSRGB, uint8_t inSwizzle)
{
    auto data_ptr = inAsset->GetData();
    const auto header_ptr = inAsset->GetHeader();

    const auto mipmap_levels = header_ptr->dwMipMapCount;
    const auto debug_name = inAsset->GetPath().wstring();

    auto texture = m_Device.GetTexture(m_Device.CreateTexture(Texture::Desc
    {
        .swizzle        = inSwizzle,
        .format         = inIsSRGB ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM,
        .width          = header_ptr->dwWidth,
        .height         = header_ptr->dwHeight,
        .mipLevels      = mipmap_levels,
        .usage          = Texture::SHADER_READ_ONLY,
        .debugName      = debug_name.c_str()
    }));

    auto& cmd_list = m_Renderer.StartSingleSubmit();

    for (uint32_t mip = 0; mip < mipmap_levels; mip++)
    {
        const auto dimensions = glm::ivec2(std::max(header_ptr->dwWidth >> mip, 1ul), std::max(header_ptr->dwHeight >> mip, 1ul));

        m_StagingHeap.StageTexture(cmd_list, texture.GetD3D12Resource(), mip, data_ptr);

        data_ptr += dimensions.x * dimensions.y;
    }

    m_Renderer.FlushSingleSubmit(m_Device, cmd_list);

    return texture.GetView().ToIndex();
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


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


RTTI_DEFINE_TYPE(GBufferData)       {}
RTTI_DEFINE_TYPE(GBufferDebugData)  {}
RTTI_DEFINE_TYPE(GrassData)         {}
RTTI_DEFINE_TYPE(RTShadowMaskData)  {}
RTTI_DEFINE_TYPE(RTAOData)          {}
RTTI_DEFINE_TYPE(ReflectionsData)   {}
RTTI_DEFINE_TYPE(DownsampleData)    {}
RTTI_DEFINE_TYPE(LightingData)      {}
RTTI_DEFINE_TYPE(FSR2Data)          {}
RTTI_DEFINE_TYPE(XeSSData)          {}
RTTI_DEFINE_TYPE(DLSSData)          {}
RTTI_DEFINE_TYPE(ProbeTraceData)    {}
RTTI_DEFINE_TYPE(ProbeUpdateData)   {}
RTTI_DEFINE_TYPE(PathTraceData)     {}
RTTI_DEFINE_TYPE(ProbeDebugData)    {}
RTTI_DEFINE_TYPE(DebugLinesData)    {}
RTTI_DEFINE_TYPE(TAAResolveData)    {}
RTTI_DEFINE_TYPE(ComposeData)       {}
RTTI_DEFINE_TYPE(PreImGuiData)      {}
RTTI_DEFINE_TYPE(ImGuiData)         {}

/*

const T& AddPass(RenderGraph& inRenderGraph, Device& inDevice) {
    return inRenderGraph.AddGraphicsPass<T>(pass_name, inDevice,
    [&](IRenderPass* inRenderPass, T& inData) {  },
    [](T& inData, CommandList& inCmdList) {   });
}

*/


const GBufferData& AddGBufferPass(RenderGraph& inRenderGraph, Device& inDevice, const Scene& inScene)
{
    return inRenderGraph.AddGraphicsPass<GBufferData>("GBUFFER PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, GBufferData& inData)
    {
        inData.mRenderTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = L"GBUFFER_RENDER"
        });

        inData.mVelocityTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = L"GBUFFER_VELOCITY"
        });

        inData.mDepthTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::DEPTH_STENCIL_TARGET,
            .debugName = L"GBUFFER_DEPTH"
        });

        ioRGBuilder.RenderTarget(inData.mRenderTexture); // SV_Target0
        ioRGBuilder.RenderTarget(inData.mVelocityTexture); // SV_Target1
        ioRGBuilder.DepthStencilTarget(inData.mDepthTexture);

        CD3DX12_SHADER_BYTECODE vertex_shader, pixel_shader;
        g_SystemShaders.mGBufferShader.GetGraphicsProgram(vertex_shader, pixel_shader);

        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, vertex_shader, pixel_shader);
        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(inData.mPipeline.GetAddressOf()));
        inData.mPipeline->SetName(L"PSO_GBUFFER");
    },

    [&inRenderGraph, &inDevice, &inScene](GBufferData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        const auto& viewport = inRenderGraph.GetViewport();
        inCmdList.SetViewportAndScissor(viewport);
        inCmdList->SetPipelineState(inData.mPipeline.Get());

        const auto clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        inCmdList->ClearDepthStencilView(inDevice.GetCPUDescriptorHandle(inResources.GetTexture(inData.mDepthTexture)), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
        inCmdList->ClearRenderTargetView(inDevice.GetCPUDescriptorHandle(inResources.GetTexture(inData.mRenderTexture)), glm::value_ptr(clear_color), 0, nullptr);
        inCmdList->ClearRenderTargetView(inDevice.GetCPUDescriptorHandle(inResources.GetTexture(inData.mVelocityTexture)), glm::value_ptr(clear_color), 0, nullptr);

        for (const auto& [entity, transform, mesh] : inScene.Each<Transform, Mesh>())
        {
            auto material = inScene.GetPtr<Material>(mesh.material);
            //if (material && material->isTransparent)
                //continue;

            const auto& index_buffer = inDevice.GetBuffer(BufferID(mesh.indexBuffer));
            const auto& vertex_buffer = inDevice.GetBuffer(BufferID(mesh.vertexBuffer));

            const auto index_view = D3D12_INDEX_BUFFER_VIEW
            {
                .BufferLocation = index_buffer->GetGPUVirtualAddress(),
                .SizeInBytes = uint32_t(mesh.indices.size() * sizeof(mesh.indices[0])),
                .Format = DXGI_FORMAT_R32_UINT,
            };

            if (mesh.material == NULL_ENTITY)
                continue;

            if (material == nullptr)
                material = &Material::Default;

            auto root_constants = GbufferRootConstants {
                .mVertexBuffer       = inDevice.GetBindlessHeapIndex(BufferID(mesh.vertexBuffer)),
                .mAlbedoTexture      = material->gpuAlbedoMap,
                .mNormalTexture      = material->gpuNormalMap,
                .mEmissiveTexture    = material->gpuEmissiveMap,
                .mMetallicTexture    = material->gpuMetallicMap,
                .mRoughnessTexture   = material->gpuRoughnessMap,
                .mRoughness          = material->roughness,
                .mMetallic           = material->metallic,
                .mAlbedo             = material->albedo,
                .mWorldTransform     = transform.worldTransform,
                .mInvWorldTransform  = glm::inverse(transform.worldTransform)
            };

            inCmdList->SetGraphicsRoot32BitConstants(0, sizeof(root_constants) / sizeof(DWORD), &root_constants, 0);

            inCmdList->IASetIndexBuffer(&index_view);

            if (entity == inData.mActiveEntity)
            {
                // do stencil stuff?
            }

            inCmdList->DrawIndexedInstanced(mesh.indices.size(), 1, 0, 0, 0);
        }
    });
}



const GBufferDebugData& AddGBufferDebugPass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferData& inGBufferData, EDebugTexture inDebugTexture)
{
    return inRenderGraph.AddGraphicsPass<GBufferDebugData>("GBUFFER DEBUG PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, GBufferDebugData& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = L"GBUFFER_DEBUG"
        });

        ioRGBuilder.RenderTarget(inData.mOutputTexture);

        switch (inDebugTexture)
        {
            case DEBUG_TEXTURE_GBUFFER_DEPTH:     inData.mInputTextureSRV = ioRGBuilder.Read(inGBufferData.mDepthTexture);  break;
            case DEBUG_TEXTURE_GBUFFER_ALBEDO:    inData.mInputTextureSRV = ioRGBuilder.Read(inGBufferData.mRenderTexture); break;
            case DEBUG_TEXTURE_GBUFFER_NORMALS:   inData.mInputTextureSRV = ioRGBuilder.Read(inGBufferData.mRenderTexture); break;
            case DEBUG_TEXTURE_GBUFFER_METALLIC:  inData.mInputTextureSRV = ioRGBuilder.Read(inGBufferData.mRenderTexture); break;
            case DEBUG_TEXTURE_GBUFFER_ROUGHNESS: inData.mInputTextureSRV = ioRGBuilder.Read(inGBufferData.mRenderTexture); break;
            default: assert(false);
        }

        CD3DX12_SHADER_BYTECODE vertex_shader, pixel_shader;
        switch (inDebugTexture)
        {
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

    [&inRenderGraph, &inDevice](GBufferDebugData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.SetViewportAndScissor(inRenderGraph.GetViewport());
        inCmdList.PushGraphicsConstants(GbufferDebugRootConstants
        {
            .mTexture   = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mInputTextureSRV)),
            .mFarPlane  = inRenderGraph.GetViewport().GetCamera().GetFar(),
            .mNearPlane = inRenderGraph.GetViewport().GetCamera().GetNear(),
        });
        inCmdList->DrawInstanced(6, 1, 0, 0);
    });
}



const RTShadowMaskData& AddShadowMaskPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferData& inGBufferData)
{
    return inRenderGraph.AddComputePass<RTShadowMaskData>("RAY TRACED SHADOWS PASS",
    [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, RTShadowMaskData& inData)
    {
        inData.mOutputTexture = inRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = L"SHADOW_MASK"
        });

        inRGBuilder.Write(inData.mOutputTexture);

        inData.mGbufferDepthTextureSRV = inRGBuilder.Read(inGBufferData.mDepthTexture);
        inData.mGBufferRenderTextureSRV = inRGBuilder.Read(inGBufferData.mRenderTexture);

        CD3DX12_SHADER_BYTECODE compute_shader;
        g_SystemShaders.mRTShadowsShader.GetComputeProgram(compute_shader);
        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, compute_shader);

        gThrowIfFailed(inDevice->CreateComputePipelineState(&state, IID_PPV_ARGS(&inData.mPipeline)));
        inData.mPipeline->SetName(L"PSO_RT_SHADOWS");
    },

    [&inRenderGraph, &inDevice, &inScene](RTShadowMaskData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        auto& viewport = inRenderGraph.GetViewport();

        inCmdList.PushComputeConstants(ShadowMaskRootConstants
        {
            .mShadowMaskTexture    = inDevice.GetBindlessHeapIndex(inResources.GetTexture(inData.mOutputTexture)),
            .mGbufferDepthTexture  = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mGbufferDepthTextureSRV)),
            .mGbufferRenderTexture = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mGBufferRenderTextureSRV)),
            .mTLAS                 = inDevice.GetBindlessHeapIndex(inScene.GetTLASDescriptor(inDevice)),
            .mDispatchSize         = viewport.size
        });

        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->Dispatch(viewport.size.x / 8, viewport.size.y / 8, 1);
    });
}



const RTAOData& AddAmbientOcclusionPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferData& inGbufferData)
{
    return inRenderGraph.AddComputePass<RTAOData>("RAY TRACED AO PASS",
    [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, RTAOData& inData)
    {
        inData.mOutputTexture = inRGBuilder.Create(Texture::Desc{
            .format = DXGI_FORMAT_R32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = L"AO_MASK"
        });

        inRGBuilder.Write(inData.mOutputTexture);

        inData.mGbufferDepthTextureSRV = inRGBuilder.Read(inGbufferData.mDepthTexture);
        inData.mGBufferRenderTextureSRV = inRGBuilder.Read(inGbufferData.mRenderTexture);

        CD3DX12_SHADER_BYTECODE compute_shader;
        g_SystemShaders.mRTAmbientOcclusionShader.GetComputeProgram(compute_shader);
        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, compute_shader);

        gThrowIfFailed(inDevice->CreateComputePipelineState(&state, IID_PPV_ARGS(&inData.mPipeline)));
        inData.mPipeline->SetName(L"PSO_RTAO");
    },

    [&inRenderGraph, &inDevice, &inScene](RTAOData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        auto& viewport = inRenderGraph.GetViewport();

        inCmdList.PushComputeConstants(AmbientOcclusionRootConstants
        {
            .mParams                = inData.mParams,
            .mAOmaskTexture         = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mOutputTexture)),
            .mGbufferDepthTexture   = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mGbufferDepthTextureSRV)),
            .mGbufferRenderTexture  = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mGBufferRenderTextureSRV)),
            .mTLAS                  = inDevice.GetBindlessHeapIndex(inScene.GetTLASDescriptor(inDevice)),
            .mDispatchSize          = viewport.size
        });

        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->Dispatch(viewport.size.x / 8, viewport.size.y / 8, 1);
    });
}



const GrassData& AddGrassRenderPass(RenderGraph& inGraph, Device& inDevice, const GBufferData& inGBufferData)
{
    return inGraph.AddGraphicsPass<GrassData>("GRASS DRAW PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, GrassData& inData)
    {
        inData.mRenderTextureSRV = ioRGBuilder.RenderTarget(inGBufferData.mRenderTexture);
        inData.mDepthTextureSRV  = ioRGBuilder.DepthStencilTarget(inGBufferData.mDepthTexture);

        CD3DX12_SHADER_BYTECODE vertex_shader, pixel_shader;
        g_SystemShaders.mGrassShader.GetGraphicsProgram(vertex_shader, pixel_shader);
        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, vertex_shader, pixel_shader);

        pso_state.InputLayout = {};
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(&inData.mPipeline));
        inData.mPipeline->SetName(L"PSO_GRASS_DRAW");

        inData.mRenderConstants = GrassRenderRootConstants
        {
            .mBend = 0.0f,
            .mTilt = 0.0f,
            .mWindDirection = glm::vec2(0.0f, -1.0f)
        };
    },

    [](GrassData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        const auto blade_vertex_count = 15;

        inCmdList.PushGraphicsConstants(inData.mRenderConstants);

        inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->DrawInstanced(blade_vertex_count, 65536, 0, 0);
        inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    });
}



const ReflectionsData& AddReflectionsPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferData& inGBufferData)
{
    return inRenderGraph.AddComputePass<ReflectionsData>("RT REFLECTION TRACE PASS",
    [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, ReflectionsData& inData)
    {
        inData.mOutputTexture = inRGBuilder.Create(Texture::Desc
        {
            .format     = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width      = inRenderGraph.GetViewport().size.x,
            .height     = inRenderGraph.GetViewport().size.y,
            .mipLevels  = 0, // let it auto-calculate the nr of mips
            .usage      = Texture::Usage::SHADER_READ_WRITE,
            .debugName = L"RT_REFLECTIONS"
        });

        inRGBuilder.Write(inData.mOutputTexture);

        inData.mGBufferDepthTextureSRV = inRGBuilder.Read(inGBufferData.mDepthTexture);
        inData.mGbufferRenderTextureSRV = inRGBuilder.Read(inGBufferData.mRenderTexture);

        CD3DX12_SHADER_BYTECODE compute_shader;
        g_SystemShaders.mRTReflectionsShader.GetComputeProgram(compute_shader);
        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, compute_shader);

        gThrowIfFailed(inDevice->CreateComputePipelineState(&state, IID_PPV_ARGS(inData.mPipeline.GetAddressOf())));
        inData.mPipeline->SetName(L"PSO_RT_REFLECTIONS");
    },

    [&inRenderGraph, &inDevice, &inScene](ReflectionsData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        auto& viewport = inRenderGraph.GetViewport();

        inCmdList.PushComputeConstants(ReflectionsRootConstants {
            .mTLAS                  = inDevice.GetBindlessHeapIndex(inScene.GetTLASDescriptor(inDevice)),
            .mInstancesBuffer       = inDevice.GetBindlessHeapIndex(inScene.GetInstancesDescriptor(inDevice)),
            .mMaterialsBuffer       = inDevice.GetBindlessHeapIndex(inScene.GetMaterialsDescriptor(inDevice)),
            .mShadowMaskTexture     = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mOutputTexture)),
            .mGbufferDepthTexture   = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mGBufferDepthTextureSRV)),
            .mGbufferRenderTexture  = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mGbufferRenderTextureSRV)),
            .mDispatchSize          = viewport.size
        });

        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->Dispatch(viewport.size.x / 8, viewport.size.y / 8, 1);
    });
}



const PathTraceData& AddPathTracePass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene)
{
    return inRenderGraph.AddComputePass<PathTraceData>("PATH TRACE PASS",
    [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, PathTraceData& inData)
    {
        inData.mOutputTexture = inRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = L"RT_PATH_TRACE_RESULT"
        });

        inData.mAccumulationTexture = inRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = L"RT_PATH_TRACE_ACCUMULATION"
        });

        inRGBuilder.Write(inData.mOutputTexture);
        inRGBuilder.Write(inData.mAccumulationTexture);

        CD3DX12_SHADER_BYTECODE compute_shader;
        g_SystemShaders.mRTPathTraceShader.GetComputeProgram(compute_shader);
        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, compute_shader);

        gThrowIfFailed(inDevice->CreateComputePipelineState(&state, IID_PPV_ARGS(inData.mPipeline.GetAddressOf())));
        inData.mPipeline->SetName(L"PSO_RT_PATH_TRACE");
    },

    [&inRenderGraph, &inDevice, &inScene](PathTraceData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        auto& viewport = inRenderGraph.GetViewport();

        inCmdList.PushComputeConstants(PathTraceRootConstants
        {
            .mTLAS                  = inDevice.GetBindlessHeapIndex(inScene.GetTLASDescriptor(inDevice)),
            .mBounces               = inData.mBounces,
            .mInstancesBuffer       = inDevice.GetBindlessHeapIndex(inScene.GetInstancesDescriptor(inDevice)),
            .mMaterialsBuffer       = inDevice.GetBindlessHeapIndex(inScene.GetMaterialsDescriptor(inDevice)),
            .mDispatchSize          = viewport.size,
            .mResultTexture         = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mOutputTexture)),
            .mAccumulationTexture   = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mAccumulationTexture)),
            .mAlpha                 = inData.mAlpha
        });

        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->Dispatch(viewport.size.x / 8, viewport.size.y / 8, 1);
    });
}



const DownsampleData& AddDownsamplePass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inSourceTexture)
{
    return inRenderGraph.AddComputePass<DownsampleData>("DOWNSAMPLE PASS",
    [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, DownsampleData& inData)
    {
        inData.mGlobalAtomicBuffer = inRGBuilder.Create(Buffer::Desc
        {
            .size   = sizeof(uint32_t),
            .stride = sizeof(uint32_t),
            .usage  = Buffer::Usage::SHADER_READ_WRITE,
            .debugName = L"SPD_ATOMIC_UINT_BUFFER"
        });

        inData.mSourceTextureUAV = inRGBuilder.Write(inSourceTexture);
        /* inData.mGlobalAtomicBuffer */ inRGBuilder.Write(inData.mGlobalAtomicBuffer);

        auto texture_desc = inRGBuilder.GetResourceDesc(inSourceTexture);
        assert(texture_desc.mResourceType == RESOURCE_TYPE_TEXTURE);

        const auto nr_of_mips = gSpdCaculateMipCount(texture_desc.mTextureDesc.width, texture_desc.mTextureDesc.height);

        assert(false); // TODO: RenderGraphBuilder::Write with subresource specifier
        for (auto mip = 0u; mip < nr_of_mips; mip++)
        {
            // inRGBuilder.Write(inSourceTexture, mip);
        }

        CD3DX12_SHADER_BYTECODE compute_shader;
        g_SystemShaders.mDownsampleShader.GetComputeProgram(compute_shader);
        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, compute_shader);

        inDevice->CreateComputePipelineState(&state, IID_PPV_ARGS(inData.mPipeline.GetAddressOf()));
        inData.mPipeline->SetName(L"PSO_DOWNSAMPLE");
    },

    [&inRenderGraph, &inDevice](DownsampleData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        const auto& texture = inDevice.GetTexture(inRGResources.GetTextureView(inData.mSourceTextureUAV));
        const auto rect_info = glm::uvec4(0u, 0u, texture->GetDesc().Width, texture->GetDesc().Height);

        glm::uvec2 work_group_offset, dispatchThreadGroupCountXY, numWorkGroupsAndMips;
        work_group_offset[0] = rect_info[0] / 64; // rectInfo[0] = left
        work_group_offset[1] = rect_info[1] / 64; // rectInfo[1] = top

        auto endIndexX = ( rect_info[0] + rect_info[2] - 1 ) / 64; // rectInfo[0] = left, rectInfo[2] = width
        auto endIndexY = ( rect_info[1] + rect_info[3] - 1 ) / 64; // rectInfo[1] = top, rectInfo[3] = height

        dispatchThreadGroupCountXY[0] = endIndexX + 1 - work_group_offset[0];
        dispatchThreadGroupCountXY[1] = endIndexY + 1 - work_group_offset[1];

        numWorkGroupsAndMips[0] = ( dispatchThreadGroupCountXY[0] ) * ( dispatchThreadGroupCountXY[1] );
        numWorkGroupsAndMips[1] = gSpdCaculateMipCount(rect_info[2], rect_info[3]);

        const auto& atomic_buffer = inDevice.GetBuffer(inData.mGlobalAtomicBuffer);

        auto root_constants = SpdRootConstants {
            .mNrOfMips = numWorkGroupsAndMips[1],
            .mNrOfWorkGroups = numWorkGroupsAndMips[0],
            .mGlobalAtomicBuffer = inDevice.GetBindlessHeapIndex(atomic_buffer.GetDescriptor()),
            .mWorkGroupOffset = work_group_offset,
        };

        const auto mips_ptr = &root_constants.mTextureMip0;

        for (auto mip = 0u; mip < numWorkGroupsAndMips[1]; mip++)
            mips_ptr[mip] = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mSourceTextureMipsUAVs[mip]));

        static auto first_run = true;

        if (first_run)
        {
            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(atomic_buffer.GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);

            inCmdList->ResourceBarrier(1, &barrier);

            const auto param = D3D12_WRITEBUFFERIMMEDIATE_PARAMETER { .Dest = atomic_buffer->GetGPUVirtualAddress(), .Value = 0 };
            inCmdList->WriteBufferImmediate(1, &param, nullptr);

            barrier = CD3DX12_RESOURCE_BARRIER::Transition(atomic_buffer.GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            inCmdList->ResourceBarrier(1, &barrier);

            first_run = false;
        }

        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.PushComputeConstants(root_constants);
        inCmdList->Dispatch(dispatchThreadGroupCountXY.x, dispatchThreadGroupCountXY.y, 1);
    });
}



const ProbeTraceData& AddProbeTracePass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene)
{
    return inRenderGraph.AddComputePass<ProbeTraceData>("GI PROBE TRACE PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ProbeTraceData& inData)
    {
        const auto total_probe_count = inData.mDDGIData.mProbeCount.x * inData.mDDGIData.mProbeCount.y * inData.mDDGIData.mProbeCount.z;

        inData.mRaysDepthTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16_FLOAT,
            .width  = DDGI_RAYS_PER_PROBE,
            .height = uint32_t(total_probe_count),
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = L"DDGI_TRACE_DEPTH"
        });

        inData.mRaysIrradianceTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R11G11B10_FLOAT,
            .width  = DDGI_RAYS_PER_PROBE,
            .height = uint32_t(total_probe_count),
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = L"DDGI_TRACE_IRRADIANCE"
        });

        ioRGBuilder.Write(inData.mRaysDepthTexture);
        ioRGBuilder.Write(inData.mRaysIrradianceTexture);

        CD3DX12_SHADER_BYTECODE compute_shader;
        g_SystemShaders.mProbeTraceShader.GetComputeProgram(compute_shader);
        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, compute_shader);

        gThrowIfFailed(inDevice->CreateComputePipelineState(&pso_state, IID_PPV_ARGS(&inData.mPipeline)));
        inData.mPipeline->SetName(L"PSO_PROBE_TRACE");
    },

    [&inRenderGraph, &inDevice, &inScene](ProbeTraceData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        auto Index3Dto1D = [](UVec3 inIndex, UVec3 inCount)
        {
            return inIndex.x + inIndex.y * inCount.x + inIndex.z * inCount.x * inCount.y;
        };

        if (inScene->Count<DDGISceneSettings>())
        {
            const auto& ddgi_entity = inScene->GetEntities<DDGISceneSettings>()[0];
            const auto& ddgi_settings = inScene->Get<DDGISceneSettings>(ddgi_entity);
            const auto& ddgi_transform = inScene->Get<Transform>(ddgi_entity);

            inData.mDDGIData.mProbeCount = ddgi_settings.mDDGIProbeCount;
            inData.mDDGIData.mProbeSpacing = ddgi_settings.mDDGIProbeSpacing;
            inData.mDDGIData.mCornerPosition = ddgi_transform.position;
        }

        inData.mRandomRotationMatrix = gRandomOrientation();
        inData.mDDGIData.mRaysDepthTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mRaysDepthTexture));
        inData.mDDGIData.mRaysIrradianceTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mRaysIrradianceTexture));

        inCmdList.PushComputeConstants(ProbeTraceRootConstants
        {
            .mInstancesBuffer = inDevice.GetBindlessHeapIndex(inScene.GetInstancesDescriptor(inDevice)),
            .mMaterialsBuffer = inDevice.GetBindlessHeapIndex(inScene.GetMaterialsDescriptor(inDevice)),
            .mTLAS = inDevice.GetBindlessHeapIndex(inScene.GetTLASDescriptor(inDevice)),
            .mDebugProbeIndex = Index3Dto1D(inData.mDebugProbe, inData.mDDGIData.mProbeCount),
            .mDDGIData = inData.mDDGIData,
            .mRandomRotationMatrix = inData.mRandomRotationMatrix
        });

        const auto total_probe_count = inData.mDDGIData.mProbeCount.x * inData.mDDGIData.mProbeCount.y * inData.mDDGIData.mProbeCount.z;

        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->Dispatch(DDGI_RAYS_PER_PROBE / DDGI_WAVE_SIZE, total_probe_count, 1);
    });
}



const ProbeUpdateData& AddProbeUpdatePass(RenderGraph& inRenderGraph, Device& inDevice, const ProbeTraceData& inTraceData)
{
    return inRenderGraph.AddComputePass<ProbeUpdateData>("GI PROBE UPDATE PASS",

    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ProbeUpdateData& inData)
    {
        inData.mDDGIData = inTraceData.mDDGIData;
        const auto total_probe_count = inData.mDDGIData.mProbeCount.x * inData.mDDGIData.mProbeCount.y * inData.mDDGIData.mProbeCount.z;

        inData.mProbesDepthTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16_FLOAT,
            .width  = uint32_t(DDGI_DEPTH_TEXELS * DDGI_PROBES_PER_ROW),
            .height = uint32_t(DDGI_DEPTH_TEXELS * ( total_probe_count / DDGI_PROBES_PER_ROW )),
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = L"DDGI_UPDATE_DEPTH"
        });

        inData.mProbesIrradianceTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = uint32_t(DDGI_IRRADIANCE_TEXELS * DDGI_PROBES_PER_ROW),
            .height = uint32_t(DDGI_IRRADIANCE_TEXELS * ( total_probe_count / DDGI_PROBES_PER_ROW )),
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = L"DDGI_UPDATE_IRRADIANCE"
        });

        ioRGBuilder.Write(inData.mProbesDepthTexture);
        ioRGBuilder.Write(inData.mProbesIrradianceTexture);

        inData.mRaysDepthTextureSRV = ioRGBuilder.Read(inTraceData.mRaysDepthTexture);
        inData.mRaysIrradianceTextureSRV = ioRGBuilder.Read(inTraceData.mRaysIrradianceTexture);

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

    [&inDevice, &inTraceData](ProbeUpdateData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        inData.mDDGIData                            = inTraceData.mDDGIData;
        inData.mDDGIData.mRaysDepthTexture          = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mRaysDepthTextureSRV));
        inData.mDDGIData.mProbesDepthTexture        = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mProbesDepthTexture));
        inData.mDDGIData.mRaysIrradianceTexture     = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mRaysIrradianceTextureSRV));
        inData.mDDGIData.mProbesIrradianceTexture   = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mProbesIrradianceTexture));
        
        const auto irradiance_texture = inDevice.GetTexture(inRGResources.GetTexture(inData.mProbesIrradianceTexture));

        inCmdList->SetPipelineState(inData.mIrradiancePipeline.Get());
        
        inCmdList.PushComputeConstants(ProbeUpdateRootConstants
        {
            .mDDGIData = inData.mDDGIData,
            .mRandomRotationMatrix = inTraceData.mRandomRotationMatrix
        });

        //inCmdList->SetPipelineState(inData.mDepthPipeline.Get());
        //const auto depth_texture = inDevice.GetTexture(inData.mProbesDepthTexture.mCreatedTexture);
        //inCmdList->Dispatch(depth_texture.GetDesc().width / DDGI_DEPTH_TEXELS, depth_texture.GetDesc().height / DDGI_DEPTH_TEXELS, 1);

        inCmdList->Dispatch(irradiance_texture.GetDesc().width / DDGI_IRRADIANCE_TEXELS, irradiance_texture.GetDesc().height / DDGI_IRRADIANCE_TEXELS, 1);
    });
}



const ProbeDebugData& AddProbeDebugPass(RenderGraph& inRenderGraph, Device& inDevice, const ProbeTraceData& inTraceData, const ProbeUpdateData& inUpdateData, RenderGraphResourceID inRenderTarget, RenderGraphResourceID inDepthTarget)
{
    return inRenderGraph.AddGraphicsPass<ProbeDebugData>("GI PROBE DEBUG PASS",

    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ProbeDebugData& inData)
    {
        gGenerateSphere(inData.mProbeMesh, 0.25f, 32u, 32u);

        const auto vertices = inData.mProbeMesh.GetInterleavedVertices();
        const auto vertices_size = vertices.size() * sizeof(vertices[0]);
        const auto indices_size = inData.mProbeMesh.indices.size() * sizeof(inData.mProbeMesh.indices[0]);

        inData.mProbeMesh.indexBuffer = inDevice.CreateBuffer(Buffer::Desc
        {
            .size     = uint32_t(indices_size),
            .stride   = sizeof(uint32_t) * 3,
            .usage    = Buffer::Usage::INDEX_BUFFER,
            .mappable = true
        }).ToIndex();

        inData.mProbeMesh.vertexBuffer = inDevice.CreateBuffer(Buffer::Desc
        {
            .size     = uint32_t(vertices_size),
            .stride   = sizeof(Vertex),
            .usage    = Buffer::Usage::VERTEX_BUFFER,
            .mappable = true
        }).ToIndex();

        {
            auto& index_buffer = inDevice.GetBuffer(BufferID(inData.mProbeMesh.indexBuffer));
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

        inData.mDDGIData = inUpdateData.mDDGIData;
        
        inData.mRenderTargetRTV = ioRGBuilder.RenderTarget(inRenderTarget);
        inData.mDepthTargetDSV = ioRGBuilder.DepthStencilTarget(inDepthTarget);

        inData.mProbesDepthTextureSRV = ioRGBuilder.Read(inUpdateData.mProbesDepthTexture);
        inData.mProbesIrradianceTextureSRV = ioRGBuilder.Read(inUpdateData.mProbesIrradianceTexture);

        CD3DX12_SHADER_BYTECODE vertex_shader, pixel_shader;
        g_SystemShaders.mProbeDebugShader.GetGraphicsProgram(vertex_shader, pixel_shader);
        auto pso_desc = inDevice.CreatePipelineStateDesc(inRenderPass, vertex_shader, pixel_shader);

        gThrowIfFailed(inDevice->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(inData.mPipeline.GetAddressOf())));
        inData.mPipeline->SetName(L"PSO_PROBE_DEBUG");
    },

    [&inRenderGraph, &inDevice, &inUpdateData](ProbeDebugData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.SetViewportAndScissor(inRenderGraph.GetViewport());

        inData.mDDGIData = inUpdateData.mDDGIData;
        inData.mDDGIData.mProbesDepthTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mProbesDepthTextureSRV));
        inData.mDDGIData.mProbesIrradianceTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mProbesIrradianceTextureSRV));
        
        inCmdList.PushGraphicsConstants(inData.mDDGIData);

        inCmdList.BindVertexAndIndexBuffers(inDevice, inData.mProbeMesh);
        
        const auto total_probe_count = inData.mDDGIData.mProbeCount.x * inData.mDDGIData.mProbeCount.y * inData.mDDGIData.mProbeCount.z;
        inCmdList->DrawIndexedInstanced(inData.mProbeMesh.indices.size(), total_probe_count, 0, 0, 0);
    });
}



const DebugLinesData& AddDebugLinesPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inRenderTarget, RenderGraphResourceID inDepthTarget)
{
    return inRenderGraph.AddGraphicsPass<DebugLinesData>("DEBUG LINES PASS",

    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, DebugLinesData& inData)
    {
        inData.mVertexBuffer = ioRGBuilder.Create(Buffer::Desc
        {
            .size   = sizeof(float4) * UINT16_MAX,
            .stride = sizeof(float4),
            .usage  = Buffer::Usage::SHADER_READ_WRITE,
            .debugName = L"DEBUG_LINES_VERTEX_BUFFER"
        });


        inData.mIndirectArgsBuffer = ioRGBuilder.Create(Buffer::Desc
        {
            .size     = sizeof(D3D12_DRAW_ARGUMENTS),
            .usage    = Buffer::Usage::SHADER_READ_WRITE,
            // .viewDesc = args_srv_desc
            .debugName = L"DEBUG_LINES_INDIRECT_ARGS_BUFFER"
        });

        const auto render_target = ioRGBuilder.RenderTarget(inRenderTarget);
        const auto depth_target  = ioRGBuilder.DepthStencilTarget(inDepthTarget);

        auto indirect_args = D3D12_INDIRECT_ARGUMENT_DESC
        {
            .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW
        };

        const auto cmdsig_desc = D3D12_COMMAND_SIGNATURE_DESC
        {
            .ByteStride = sizeof(D3D12_DRAW_ARGUMENTS),
            .NumArgumentDescs = 1,
            .pArgumentDescs = &indirect_args
        };

        inDevice->CreateCommandSignature(&cmdsig_desc, nullptr, IID_PPV_ARGS(inData.mCommandSignature.GetAddressOf()));

        constexpr auto vertex_layout = std::array 
        {
            D3D12_INPUT_ELEMENT_DESC { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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

    [&inDevice](DebugLinesData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        auto vertices_buffer_resource = inDevice.GetD3D12Resource(inRGResources.GetBuffer(inData.mVertexBuffer));
        auto indirect_args_buffer_resource = inDevice.GetD3D12Resource(inRGResources.GetBuffer(inData.mIndirectArgsBuffer));

        const auto vertices_entry_barrier = CD3DX12_RESOURCE_BARRIER::Transition(vertices_buffer_resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        const auto indirect_args_entry_barrier = CD3DX12_RESOURCE_BARRIER::Transition(indirect_args_buffer_resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        const auto entry_barriers = std::array { vertices_entry_barrier, indirect_args_entry_barrier };

        const auto vertices_exit_barrier = CD3DX12_RESOURCE_BARRIER::Transition(vertices_buffer_resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        const auto indirect_args_exit_barrier = CD3DX12_RESOURCE_BARRIER::Transition(indirect_args_buffer_resource, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        const auto exit_barriers = std::array { vertices_exit_barrier, indirect_args_exit_barrier };

        const auto& vertex_buffer = inDevice.GetBuffer(inRGResources.GetBuffer(inData.mVertexBuffer));

        const auto vertex_view = D3D12_VERTEX_BUFFER_VIEW {
            .BufferLocation = vertex_buffer->GetGPUVirtualAddress(),
            .SizeInBytes = uint32_t(vertex_buffer->GetDesc().Width),
            .StrideInBytes = sizeof(float4)
        };

        inCmdList->ResourceBarrier(entry_barriers.size(), entry_barriers.data());
        inCmdList->IASetVertexBuffers(0, 1, &vertex_view);
        inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->ExecuteIndirect(inData.mCommandSignature.Get(), 1, indirect_args_buffer_resource, 0, nullptr, 0);
        inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        inCmdList->ResourceBarrier(exit_barriers.size(), exit_barriers.data());
    });
}



const LightingData& AddLightingPass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferData& inGBufferData, const RTShadowMaskData& inShadowMaskData, const ReflectionsData& inReflectionsData, RenderGraphResourceID inAOTexture, const ProbeUpdateData& inProbeData)
{
    return inRenderGraph.AddGraphicsPass<LightingData>("DEFERRED LIGHTING PASS",

    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, LightingData& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = L"LIGHT_OUTPUT"
        });

        ioRGBuilder.RenderTarget(inData.mOutputTexture);

        inData.mDDGIData = inProbeData.mDDGIData;
        inData.mShadowMaskTextureSRV        = ioRGBuilder.Read(inShadowMaskData.mOutputTexture);
        inData.mReflectionsTextureSRV       = ioRGBuilder.Read(inReflectionsData.mOutputTexture);
        inData.mGBufferDepthTextureSRV      = ioRGBuilder.Read(inGBufferData.mDepthTexture);
        inData.mGBufferRenderTextureSRV     = ioRGBuilder.Read(inGBufferData.mRenderTexture);
        inData.mAmbientOcclusionTextureSRV  = ioRGBuilder.Read(inAOTexture);
        inData.mProbesDepthTextureSRV       = ioRGBuilder.Read(inProbeData.mProbesDepthTexture);
        inData.mProbesIrradianceTextureSRV  = ioRGBuilder.Read(inProbeData.mProbesIrradianceTexture);
        // inData.mIndirectDiffuseTexture   = inRenderPass->Read(inDiffuseGIData.mOutputTexture);

        CD3DX12_SHADER_BYTECODE vertex_shader, pixel_shader;
        g_SystemShaders.mLightingShader.GetGraphicsProgram(vertex_shader, pixel_shader);
        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, vertex_shader, pixel_shader);

        pso_state.InputLayout = {}; // clear the input layout, we generate the fullscreen triangle inside the vertex shader
        pso_state.DepthStencilState.DepthEnable = FALSE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(&inData.mPipeline));
        inData.mPipeline->SetName(L"PSO_DEFERRED_LIGHTING");
    },

    [&inRenderGraph, &inDevice, &inProbeData](LightingData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        auto root_constants = LightingRootConstants
        {
            .mShadowMaskTexture       = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mShadowMaskTextureSRV)),
            .mReflectionsTexture      = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mReflectionsTextureSRV)),
            .mGbufferDepthTexture     = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mGBufferDepthTextureSRV)),
            .mGbufferRenderTexture    = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mGBufferRenderTextureSRV)),
            .mAmbientOcclusionTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mAmbientOcclusionTextureSRV)),
            // .mIndirectDiffuseTexture    = inData.mIndirectDiffuseTexture.GetBindlessIndex(inDevice),
            .mDDGIData = inProbeData.mDDGIData
        };

        root_constants.mDDGIData.mProbesDepthTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mProbesDepthTextureSRV));
        root_constants.mDDGIData.mProbesIrradianceTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mProbesIrradianceTextureSRV));

        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.SetViewportAndScissor(inRenderGraph.GetViewport());
        inCmdList.PushGraphicsConstants(root_constants);
        inCmdList->DrawInstanced(6, 1, 0, 0);
    });
}



const FSR2Data& AddFsrPass(RenderGraph& inRenderGraph, Device& inDevice, FfxFsr2Context& inContext, RenderGraphResourceID inColorTexture, const GBufferData& inGBufferData)
{
    return inRenderGraph.AddComputePass<FSR2Data>("FSR2 PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, FSR2Data& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::SHADER_READ_WRITE,
            .debugName = L"FSR2_OUTPUT"
        });

        ioRGBuilder.Write(inData.mOutputTexture);

        inData.mColorTextureSRV = ioRGBuilder.Read(inColorTexture);
        inData.mDepthTextureSRV = ioRGBuilder.Read(inGBufferData.mDepthTexture);
        inData.mMotionVectorTextureSRV = ioRGBuilder.Read(inGBufferData.mVelocityTexture);

        inRenderPass->SetExternal(true);
    },

    [&inRenderGraph, &inDevice, &inContext](FSR2Data& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        auto& viewport = inRenderGraph.GetViewport();

        const auto color_texture_ptr = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mColorTextureSRV));
        const auto depth_texture_ptr = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mDepthTextureSRV));
        const auto movec_texture_ptr = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mMotionVectorTextureSRV));
        const auto output_texture_ptr = inDevice.GetD3D12Resource(inResources.GetTexture(inData.mOutputTexture));

        auto fsr2_dispatch_desc = FfxFsr2DispatchDescription {};
        fsr2_dispatch_desc.commandList = ffxGetCommandListDX12(inCmdList);
        fsr2_dispatch_desc.color = ffxGetResourceDX12(&inContext, color_texture_ptr, nullptr, FFX_RESOURCE_STATE_COMPUTE_READ);
        fsr2_dispatch_desc.depth = ffxGetResourceDX12(&inContext, depth_texture_ptr, nullptr, FFX_RESOURCE_STATE_COMPUTE_READ);
        fsr2_dispatch_desc.motionVectors = ffxGetResourceDX12(&inContext, movec_texture_ptr, nullptr, FFX_RESOURCE_STATE_COMPUTE_READ);
        fsr2_dispatch_desc.output = ffxGetResourceDX12(&inContext, output_texture_ptr, nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
        fsr2_dispatch_desc.exposure = ffxGetResourceDX12(&inContext, nullptr);
        fsr2_dispatch_desc.motionVectorScale.x = float(viewport.size.x);
        fsr2_dispatch_desc.motionVectorScale.y = float(viewport.size.y);
        fsr2_dispatch_desc.reset = false;
        fsr2_dispatch_desc.enableSharpening = false;
        fsr2_dispatch_desc.sharpness = 0.0f;
        fsr2_dispatch_desc.frameTimeDelta = Timer::sToMilliseconds(inData.mDeltaTime);
        fsr2_dispatch_desc.preExposure = 1.0f;
        fsr2_dispatch_desc.renderSize.width = viewport.size.x;
        fsr2_dispatch_desc.renderSize.height = viewport.size.y;
        fsr2_dispatch_desc.cameraFar = viewport.GetCamera().GetFar();
        fsr2_dispatch_desc.cameraNear = viewport.GetCamera().GetNear();
        fsr2_dispatch_desc.cameraFovAngleVertical = glm::radians(viewport.GetFieldOfView());

        const auto jitter_phase_count = ffxFsr2GetJitterPhaseCount(viewport.size.x, viewport.size.x);
        ffxFsr2GetJitterOffset(&fsr2_dispatch_desc.jitterOffset.x, &fsr2_dispatch_desc.jitterOffset.y, inData.mFrameCounter, jitter_phase_count);
        inData.mFrameCounter = ( inData.mFrameCounter + 1 ) % jitter_phase_count;

        gThrowIfFailed(ffxFsr2ContextDispatch(&inContext, &fsr2_dispatch_desc));
    });
}



const DLSSData& AddDLSSPass(RenderGraph& inRenderGraph, Device& inDevice, NVSDK_NGX_Handle* inDLSSHandle, NVSDK_NGX_Parameter* inDLSSParams, RenderGraphResourceID inColorTexture, const GBufferData& inGBufferData)
{
    return inRenderGraph.AddComputePass<DLSSData>("DLSS PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, DLSSData& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::SHADER_READ_WRITE,
            .debugName = L"DLSS_OUTPUT"
        });

        ioRGBuilder.Write(inData.mOutputTexture);

        inData.mColorTextureSRV = ioRGBuilder.Read(inColorTexture);
        inData.mDepthTextureSRV = ioRGBuilder.Read(inGBufferData.mDepthTexture);
        inData.mMotionVectorTextureSRV = ioRGBuilder.Read(inGBufferData.mVelocityTexture);

        inRenderPass->SetExternal(true);
    },

    [&inRenderGraph, &inDevice, inDLSSHandle, inDLSSParams](DLSSData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        auto& viewport = inRenderGraph.GetViewport();

        const auto color_texture_ptr = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mColorTextureSRV));
        const auto depth_texture_ptr = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mDepthTextureSRV));
        const auto movec_texture_ptr = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mMotionVectorTextureSRV));
        const auto output_texture_ptr = inDevice.GetD3D12Resource(inResources.GetTexture(inData.mOutputTexture));
        
        NVSDK_NGX_D3D12_DLSS_Eval_Params eval_params = {};
        eval_params.Feature.pInColor = color_texture_ptr;
        eval_params.Feature.pInOutput = output_texture_ptr;
        eval_params.pInDepth = depth_texture_ptr;
        eval_params.pInMotionVectors = movec_texture_ptr;
        eval_params.InRenderSubrectDimensions.Width = viewport.size.x;
        eval_params.InRenderSubrectDimensions.Height = viewport.size.y;

        const auto jitter_phase_count = ffxFsr2GetJitterPhaseCount(viewport.size.x, viewport.size.x);
        ffxFsr2GetJitterOffset(&eval_params.InJitterOffsetX, &eval_params.InJitterOffsetY, inData.mFrameCounter, jitter_phase_count);
        inData.mFrameCounter = ( inData.mFrameCounter + 1 ) % jitter_phase_count;

        gThrowIfFailed(NGX_D3D12_EVALUATE_DLSS_EXT(inCmdList, inDLSSHandle, inDLSSParams, &eval_params));
    });
}



const XeSSData& AddXeSSPass(RenderGraph& inRenderGraph, Device& inDevice, xess_context_handle_t inContext, RenderGraphResourceID inColorTexture, const GBufferData& inGBufferData)
{
    return inRenderGraph.AddComputePass<XeSSData>("XeSS PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, XeSSData& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::SHADER_READ_WRITE,
            .debugName = L"DLSS_OUTPUT",
        });

        ioRGBuilder.Write(inData.mOutputTexture);

        inData.mColorTextureSRV = ioRGBuilder.Read(inColorTexture);
        inData.mDepthTextureSRV = ioRGBuilder.Read(inGBufferData.mDepthTexture);
        inData.mMotionVectorTextureSRV = ioRGBuilder.Read(inGBufferData.mVelocityTexture);

        inRenderPass->SetExternal(true);
    },

    [&inRenderGraph, &inDevice, inContext](XeSSData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        auto& viewport = inRenderGraph.GetViewport();

        const auto color_texture_ptr = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mColorTextureSRV));
        const auto depth_texture_ptr = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mDepthTextureSRV));
        const auto movec_texture_ptr = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mMotionVectorTextureSRV));
        const auto output_texture_ptr = inDevice.GetD3D12Resource(inResources.GetTexture(inData.mOutputTexture));

        xess_d3d12_execute_params_t exec_params = {};
        exec_params.pColorTexture    = color_texture_ptr;
        exec_params.pVelocityTexture = movec_texture_ptr;
        exec_params.pDepthTexture    = depth_texture_ptr;
        exec_params.pOutputTexture   = output_texture_ptr;
        exec_params.inputWidth       = viewport.size.x;
        exec_params.inputHeight      = viewport.size.y;

        const auto jitter_phase_count = ffxFsr2GetJitterPhaseCount(viewport.size.x, viewport.size.x);
        ffxFsr2GetJitterOffset(&exec_params.jitterOffsetX, &exec_params.jitterOffsetY, inData.mFrameCounter, jitter_phase_count);
        inData.mFrameCounter = ( inData.mFrameCounter + 1 ) % jitter_phase_count;

        gThrowIfFailed(xessD3D12Execute(inContext, inCmdList, &exec_params));
    });
}



const TAAResolveData& AddTAAResolvePass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferData& inGBufferData, RenderGraphResourceID inColorTexture, uint32_t inFrameCounter)
{
    return inRenderGraph.AddGraphicsPass<TAAResolveData>("TAA RESOLVE PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, TAAResolveData& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = L"RT_TAA_RESOLVE"
        });

        inData.mHistoryTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::SHADER_READ_ONLY,
            .debugName = L"RT_TAA_HISTORY"
        });

        ioRGBuilder.RenderTarget(inData.mOutputTexture);

        inData.mColorTextureSRV    =  ioRGBuilder.Read(inColorTexture);
        inData.mHistoryTextureSRV  =  ioRGBuilder.Read(inData.mHistoryTexture);
        inData.mDepthTextureSRV    =  ioRGBuilder.Read(inGBufferData.mDepthTexture);
        inData.mVelocityTextureSRV =  ioRGBuilder.Read(inGBufferData.mVelocityTexture);

        CD3DX12_SHADER_BYTECODE vertex_shader, pixel_shader;
        g_SystemShaders.mTAAResolveShader.GetGraphicsProgram(vertex_shader, pixel_shader);
        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, vertex_shader, pixel_shader);

        pso_state.InputLayout = {}; // clear the input layout, we generate the fullscreen triangle inside the vertex shader
        pso_state.DepthStencilState.DepthEnable = FALSE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(&inData.mPipeline));
        inData.mPipeline->SetName(L"PSO_TAA_RESOLVE");
    },

    [&inRenderGraph, &inDevice](TAAResolveData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.SetViewportAndScissor(inRenderGraph.GetViewport());

        inCmdList.PushGraphicsConstants(TAAResolveConstants {
            .mRenderSize      = inRenderGraph.GetViewport().GetSize(),
            .mRenderSizeRcp   = 1.0f / Vec2(inRenderGraph.GetViewport().GetSize()),
            .mColorTexture    = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mColorTextureSRV)),
            .mDepthTexture    = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mDepthTextureSRV)),
            .mHistoryTexture  = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mHistoryTextureSRV)),
            .mVelocityTexture = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mVelocityTextureSRV))
        });

        inCmdList->DrawInstanced(6, 1, 0, 0);

        auto result_texture_resource = inDevice.GetD3D12Resource(inResources.GetTexture(inData.mOutputTexture));
        auto history_texture_resource = inDevice.GetD3D12Resource(inResources.GetTexture(inData.mHistoryTexture));

        auto barriers = std::array
        {
            D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(result_texture_resource, gGetResourceStates(Texture::RENDER_TARGET), D3D12_RESOURCE_STATE_COPY_SOURCE)),
            D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(history_texture_resource, gGetResourceStates(Texture::SHADER_READ_ONLY), D3D12_RESOURCE_STATE_COPY_DEST))
        };
        inCmdList->ResourceBarrier(barriers.size(), barriers.data());

        const auto dest = CD3DX12_TEXTURE_COPY_LOCATION(history_texture_resource, 0);
        const auto source = CD3DX12_TEXTURE_COPY_LOCATION(result_texture_resource, 0);
        inCmdList->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);

        for (auto& barrier : barriers)
            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

        inCmdList->ResourceBarrier(barriers.size(), barriers.data());
    });
}



const ComposeData& AddComposePass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inInputTexture)
{
    return inRenderGraph.AddGraphicsPass<ComposeData>("COMPOSE PASS",

    [&](RenderGraphBuilder& inBuilder, IRenderPass* inRenderPass, ComposeData& inData)
    {
        inData.mOutputTexture = inBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = L"COMPOSE_OUTPUT"
        });

        inBuilder.RenderTarget(inData.mOutputTexture);

        inData.mInputTextureSRV = inBuilder.Read(inInputTexture);

        CD3DX12_SHADER_BYTECODE vertex_shader, pixel_shader;
        g_SystemShaders.mFinalComposeShader.GetGraphicsProgram(vertex_shader, pixel_shader);
        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, vertex_shader, pixel_shader);

        pso_state.InputLayout = {}; // clear the input layout, we generate the fullscreen triangle inside the vertex shader
        pso_state.DepthStencilState.DepthEnable = FALSE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(&inData.mPipeline));
        inData.mPipeline->SetName(L"PSO_COMPOSE");
    },

    [&inRenderGraph, &inDevice](ComposeData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.SetViewportAndScissor(inRenderGraph.GetViewport());
        
        inCmdList.PushGraphicsConstants(ComposeRootConstants {
            .mExposure = inData.mExposure,
            .mInputTexture = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mInputTextureSRV))
         });

        inCmdList->DrawInstanced(6, 1, 0, 0);
    });
}



const PreImGuiData& AddPreImGuiPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID ioDisplayTexture)
{
    return inRenderGraph.AddGraphicsPass<PreImGuiData>("PRE-IMGUI PASS",

    [&](RenderGraphBuilder& inBuilder, IRenderPass* inRenderPass, PreImGuiData& inData)
    {
        // this pass only exists to transition the final texture to a read state so ImGui can access it TODO: FIXME: pls fix
        inData.mDisplayTextureSRV = inBuilder.Read(ioDisplayTexture);
    },

    [&inRenderGraph, &inDevice](PreImGuiData& inData, const RenderGraphResources& inResources, CommandList& inCmdList) 
    { 
        /* nothing to do. */ 
    });
}



static void ImGui_ImplDX12_SetupRenderState(ImDrawData* draw_data, CommandList& inCmdList, const Buffer& inVertexBuffer, const Buffer& inIndexBuffer, ImGuiRootConstants& inRootConstants)
{
    // Setup orthographic projection matrix
    inRootConstants.mProjection = glm::ortho(
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



const ImGuiData& AddImGuiPass(RenderGraph& inRenderGraph, Device& inDevice, StagingHeap& inStagingHeap, RenderGraphResourceID inInputTexture, TextureID inBackBuffer)
{
    return inRenderGraph.AddGraphicsPass<ImGuiData>("IMGUI PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ImGuiData& inData)
    {
        constexpr size_t max_buffer_size = 65536; // Taken from Vulkan's ImGuiPass
        inData.mIndexScratchBuffer.resize(max_buffer_size);
        inData.mVertexScratchBuffer.resize(max_buffer_size);

        inData.mIndexBuffer = ioRGBuilder.Create(Buffer::Desc
        {
            .size = max_buffer_size,
            .stride = sizeof(uint16_t),
            .usage = Buffer::Usage::INDEX_BUFFER,
            .debugName = L"IMGUI_INDEX_BUFFER"
        });

        inData.mVertexBuffer = ioRGBuilder.Create(Buffer::Desc
        {
            .size = max_buffer_size,
            .stride = sizeof(ImDrawVert),
            .usage = Buffer::Usage::VERTEX_BUFFER,
            .debugName = L"IMGUI_VERTEX_BUFFER"
        });

        inData.mInputTextureSRV = ioRGBuilder.Read(inInputTexture);
        inData.mBackBufferRTV = ioRGBuilder.RenderTarget(ioRGBuilder.Import(inBackBuffer));

        static constexpr auto input_layout = std::array
        {
            D3D12_INPUT_ELEMENT_DESC { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ImDrawVert, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                D3D12_INPUT_ELEMENT_DESC { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ImDrawVert, uv), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                D3D12_INPUT_ELEMENT_DESC { "COLOR", 0, DXGI_FORMAT_R32_UINT, 0, offsetof(ImDrawVert, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        CD3DX12_SHADER_BYTECODE vertex_shader, pixel_shader;
        g_SystemShaders.mImGuiShader.GetGraphicsProgram(vertex_shader, pixel_shader);

        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, vertex_shader, pixel_shader);
        pso_state.InputLayout = D3D12_INPUT_LAYOUT_DESC
        {
            .pInputElementDescs = input_layout.data(),
            .NumElements = input_layout.size(),
        };

        pso_state.DepthStencilState.DepthEnable = FALSE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(&inData.mPipeline));
        inData.mPipeline->SetName(L"PSO_IMGUI");
    },

    [&inRenderGraph, &inDevice, &inStagingHeap, inBackBuffer](ImGuiData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        {   // manual barriers around the imported backbuffer resource, the rendergraph doesn't handle this kind of state
            auto backbuffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetD3D12Resource(inBackBuffer), D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
            inCmdList->ResourceBarrier(1, &backbuffer_barrier);
        }

        inCmdList->SetPipelineState(inData.mPipeline.Get());

        auto draw_data = ImGui::GetDrawData();
        auto idx_dst = (ImDrawIdx*)inData.mIndexScratchBuffer.data();
        auto vtx_dst = (ImDrawVert*)inData.mVertexScratchBuffer.data();

        for (auto& cmd_list : Slice(draw_data->CmdLists, draw_data->CmdListsCount))
        {
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
        auto barriers = std::array
        {
            D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetBuffer(inData.mIndexBuffer).GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST)),
                D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetBuffer(inData.mVertexBuffer).GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST))
        };

        if (index_buffer_size) nr_of_barriers++;
        if (vertex_buffer_size) nr_of_barriers++;

        if (nr_of_barriers)
            inCmdList->ResourceBarrier(nr_of_barriers, barriers.data());

        if (vertex_buffer_size)
            inStagingHeap.StageBuffer(inCmdList, inDevice.GetBuffer(inData.mVertexBuffer).GetD3D12Resource(), 0, inData.mVertexScratchBuffer.data(), vertex_buffer_size);

        if (index_buffer_size)
            inStagingHeap.StageBuffer(inCmdList, inDevice.GetBuffer(inData.mIndexBuffer).GetD3D12Resource(), 0, inData.mIndexScratchBuffer.data(), index_buffer_size);

        if (nr_of_barriers)
        {
            for (auto& barrier : barriers)
                std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

            inCmdList->ResourceBarrier(nr_of_barriers, barriers.data());
        }

        auto root_constants = ImGuiRootConstants { .mBindlessTextureIndex = 1 };
        ImGui_ImplDX12_SetupRenderState(draw_data, inCmdList, inDevice.GetBuffer(inData.mVertexBuffer), inDevice.GetBuffer(inData.mIndexBuffer), root_constants);

        auto global_vtx_offset = 0;
        auto global_idx_offset = 0;
        auto clip_off = draw_data->DisplayPos;

        for (auto& cmd_list : Slice(draw_data->CmdLists, draw_data->CmdListsCount))
        {
            for (const auto& cmd : cmd_list->CmdBuffer)
            {
                if (cmd.UserCallback)
                {
                    if (cmd.UserCallback == ImDrawCallback_ResetRenderState)
                        ImGui_ImplDX12_SetupRenderState(draw_data, inCmdList, inDevice.GetBuffer(inData.mVertexBuffer), inDevice.GetBuffer(inData.mIndexBuffer), root_constants);
                    else
                        cmd.UserCallback(cmd_list, &cmd);
                }
                else
                {
                    // IRenderer::GetImGuiTextureID writes out the bindless index into the ResourceDescriptorHeap, so we can assign that directly here
                    if (cmd.TextureId)
                        root_constants.mBindlessTextureIndex = 1;
                    else
                        root_constants.mBindlessTextureIndex = 1; // set 0 to make debugging easier (should be the blue noise texture I think)

                    inCmdList.PushGraphicsConstants(root_constants);

                    // Project scissor/clipping rectangles into framebuffer space
                    const auto clip_min = ImVec2(cmd.ClipRect.x - clip_off.x, cmd.ClipRect.y - clip_off.y);
                    const auto clip_max = ImVec2(cmd.ClipRect.z - clip_off.x, cmd.ClipRect.w - clip_off.y);
                    if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                        continue;

                    const auto scissor_rect = D3D12_RECT { (LONG)clip_min.x, (LONG)clip_min.y, (LONG)clip_max.x, (LONG)clip_max.y };
                    inCmdList->RSSetScissorRects(1, &scissor_rect);

                    inCmdList->DrawIndexedInstanced(cmd.ElemCount, 1, cmd.IdxOffset + global_idx_offset, cmd.VtxOffset + global_vtx_offset, 0);
                }
            }

            global_idx_offset += cmd_list->IdxBuffer.Size;
            global_vtx_offset += cmd_list->VtxBuffer.Size;
        }

        {
            auto backbuffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetD3D12Resource(inBackBuffer), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON);
            inCmdList->ResourceBarrier(1, &backbuffer_barrier);
        }
    });
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
        .debugName = L"IMGUI_FONT_TEXTURE"
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
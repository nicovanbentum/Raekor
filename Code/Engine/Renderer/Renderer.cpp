#include "pch.h"
#include "Renderer.h"

#include "Shared.h"
#include "Scene.h"
#include "Shader.h"
#include "Upscalers.h"
#include "RayTracing.h"
#include "RenderGraph.h"
#include "RenderPasses.h"

#include "OS.h"
#include "Iter.h"
#include "Timer.h"
#include "Profiler.h"
#include "Primitives.h"
#include "Application.h"

namespace RK::DX12 {

Renderer::Renderer(Device& inDevice, const Viewport& inViewport, SDL_Window* inWindow) :
    m_Window(inWindow),
    m_RenderGraph(inDevice, inViewport, sFrameCount)
{
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc = 
    {
        .Width       = inViewport.GetDisplaySize().x,
        .Height      = inViewport.GetDisplaySize().y,
        .Format      = sSwapchainFormat,
        .SampleDesc  = DXGI_SAMPLE_DESC {.Count = 1 },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = sFrameCount,
        .SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD,
    };

    if (inDevice.IsTearingSupported())
        swapchain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    ComPtr<IDXGIFactory4> factory = nullptr;
    gThrowIfFailed(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory)));

    SDL_SysWMinfo wmInfo = {};
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(inWindow, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;

    ComPtr<IDXGISwapChain1> swapchain = nullptr;
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
        D3D12ResourceRef rtv_resource = nullptr;
        gThrowIfFailed(m_Swapchain->GetBuffer(index, IID_PPV_ARGS(rtv_resource.GetAddressOf())));
        
        D3D12_RESOURCE_DESC rtv_resource_desc = rtv_resource->GetDesc();

        backbuffer_data.mBackBuffer = inDevice.CreateTextureView(rtv_resource, Texture::Desc
        {
            .format = rtv_resource_desc.Format,
            .width  = uint32_t(rtv_resource_desc.Width),
            .height = uint32_t(rtv_resource_desc.Height),
            .usage  = Texture::Usage::RENDER_TARGET, 
            .debugName = "BackBuffer"
        });

        backbuffer_data.mDirectCmdList = CommandList(inDevice, D3D12_COMMAND_LIST_TYPE_DIRECT, index);
        backbuffer_data.mDirectCmdList->SetName(L"RK::DX12::CommandList(DIRECT)");

        backbuffer_data.mCopyCmdList = CommandList(inDevice, D3D12_COMMAND_LIST_TYPE_DIRECT, index);
        backbuffer_data.mCopyCmdList->SetName(L"RK::DX12::CommandList(COPY)");

        rtv_resource->SetName(L"BACKBUFFER");
    }

    inDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));
    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (!m_FenceEvent)
        gThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));

    g_CVariables->CreateFn("fn_compile_psos", [this, &inDevice]()
    {
        WaitForIdle(inDevice);
        g_SystemShaders.CompilePSOs(inDevice);
        g_ThreadPool.WaitForJobs();
    });

    g_CVariables->CreateFn("fn_hotload_shaders", [this, &inDevice]()
    {
        WaitForIdle(inDevice);
        g_SystemShaders.HotLoad(inDevice);
        SetShouldResize(true);
        g_ThreadPool.WaitForJobs();
    });
}



void Renderer::OnResize(Device& inDevice, Viewport& inViewport, bool inFullScreen)
{
    PROFILE_FUNCTION_CPU();

    for (BackBufferData& bb_data : m_BackBufferData)
        inDevice.ReleaseTextureImmediate(bb_data.mBackBuffer);

    DXGI_SWAP_CHAIN_DESC desc = {};
    gThrowIfFailed(m_Swapchain->GetDesc(&desc));

    int window_width = 0, window_height = 0;
    SDL_GetWindowSize(m_Window, &window_width, &window_height);
    gThrowIfFailed(m_Swapchain->ResizeBuffers(desc.BufferCount, window_width, window_height, desc.BufferDesc.Format, desc.Flags));

    for (const auto& [index, backbuffer_data] : gEnumerate(m_BackBufferData))
    {
        D3D12ResourceRef rtv_resource = nullptr;
        gThrowIfFailed(m_Swapchain->GetBuffer(index, IID_PPV_ARGS(rtv_resource.GetAddressOf())));

        D3D12_RESOURCE_DESC rtv_resource_desc = rtv_resource->GetDesc();

        static constexpr std::array swapchain_buffer_names =
        {
            "SwapchainBuffer0",
            "SwapchainBuffer1",
            "SwapchainBuffer2"
        };

        backbuffer_data.mBackBuffer = inDevice.CreateTextureView(rtv_resource, Texture::Desc
        {
            .format = rtv_resource_desc.Format,
            .width  = uint32_t(rtv_resource_desc.Width),
            .height = uint32_t(rtv_resource_desc.Height),
            .usage  = Texture::Usage::RENDER_TARGET,
            .debugName = swapchain_buffer_names[index]
        });
    }

    m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

    if (!m_Settings.mEnableTAA && m_Upscaler.GetActiveUpscaler() != UPSCALER_NONE && m_Upscaler.GetActiveUpscalerQuality() < UPSCALER_QUALITY_COUNT)
    {
        inViewport.SetRenderSize(Upscaler::sGetRenderResolution(inViewport.GetDisplaySize(), m_Upscaler.GetActiveUpscalerQuality()));

        bool upscale_init_success = true;

        switch (m_Upscaler.GetActiveUpscaler())
        {
            case UPSCALER_FSR:
            {
                upscale_init_success = m_Upscaler.InitFSR(inDevice, inViewport);
            } break;

            case UPSCALER_DLSS: 
            {
                CommandList& cmd_list = StartSingleSubmit();

                upscale_init_success = m_Upscaler.InitDLSS(inDevice, inViewport, cmd_list);

                FlushSingleSubmit(inDevice, cmd_list);

            } break;

            case UPSCALER_XESS: 
            {
                upscale_init_success = m_Upscaler.InitXeSS(inDevice, inViewport);;
            } break;

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



void Renderer::OnRender(Application* inApp, Device& inDevice, Viewport& inViewport, RayTracedScene& inScene, IRenderInterface* inRenderInterface, float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    // Check if any of the shader sources were updated and recompile them if necessary.
    // the OS file stamp checks are expensive so we only turn this on in debug builds.
    static bool force_hotload = OS::sCheckCommandLineOption("-force_enable_hotload");
    bool need_recompile = IF_DEBUG_ELSE(g_SystemShaders.HotLoad(inDevice), force_hotload ? g_SystemShaders.HotLoad(inDevice) : false);
    if ( need_recompile )
        std::cout << std::format("Hotloaded system shaders.\n");

    static bool do_stress_test = OS::sCheckCommandLineOption("-stress_test");

    bool recompiled = false;

    if (m_ShouldResize || m_ShouldRecompile || need_recompile || (do_stress_test && m_FrameCounter > 60))
    {
        // Make sure nothing is using render targets anymore
        WaitForIdle(inDevice);

        // Resize the renderer, which recreates the swapchain backbuffers and re-inits upscalers
        if (m_ShouldResize)
            OnResize(inDevice, inViewport, inApp->IsWindowExclusiveFullscreen());

        // Recompile the renderer, super overkill. TODO: pls fix
        Recompile(inDevice, inScene, inRenderInterface);

        // Flag / Unflag
        recompiled = true;
        m_ShouldResize = false;
        m_ShouldRecompile = false;
    }

    // At this point in the frame we really need the previous frame's present job to have finished
    if (m_PresentJobPtr)
        m_PresentJobPtr->WaitCPU();

    BackBufferData& backbuffer_data = GetBackBufferData();
    uint64_t completed_value = m_Fence->GetCompletedValue();

    // make sure the backbuffer data we're about to use is no longer being used by the GPU
    if (completed_value < backbuffer_data.mFenceValue)
    {
        gThrowIfFailed(m_Fence->SetEventOnCompletion(backbuffer_data.mFenceValue, m_FenceEvent));
        WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
    }

    // at this point we know the GPU is no longer working on this frame, so free/release stuff here
     if (m_FrameCounter > 0)
    {
        inDevice.RetireUploadBuffers(backbuffer_data.mCopyCmdList);
        inDevice.RetireUploadBuffers(backbuffer_data.mDirectCmdList);
        backbuffer_data.mCopyCmdList.ReleaseTrackedResources();
        backbuffer_data.mDirectCmdList.ReleaseTrackedResources();
    }

    // clear the clear heap every 2 frames, TODO: bad design pls fix
    if (m_FrameCounter % sFrameCount == 0)
        inDevice.GetClearHeap().Clear();

    // Update the total running time of the application / renderer
    m_ElapsedTime += inDeltaTime;

    // Calculate a jittered projection matrix for TAA/FSR/DLSS/XESS
    const Camera& camera = m_RenderGraph.GetViewport().GetCamera();
    const int32_t jitter_phase_count = ffxFsr2GetJitterPhaseCount(m_RenderGraph.GetViewport().GetRenderSize().x, m_RenderGraph.GetViewport().GetDisplaySize().x);

    float jitter_offset_x = 0;
    float jitter_offset_y = 0;
    ffxFsr2GetJitterOffset(&jitter_offset_x, &jitter_offset_y, m_FrameCounter, jitter_phase_count);

    const float jitter_x = 2.0f * jitter_offset_x / (float)m_RenderGraph.GetViewport().GetRenderSize().x;
    const float jitter_y = -2.0f * jitter_offset_y / (float)m_RenderGraph.GetViewport().GetRenderSize().y;
    const Mat4x4 jitter_matrix = glm::translate(Mat4x4(1.0f), Vec3(jitter_x, jitter_y, 0));
    
    const bool enable_jitter = m_Settings.mEnableTAA || m_Upscaler.GetActiveUpscaler();
    const Mat4x4 final_proj_matrix = enable_jitter ? jitter_matrix * camera.GetProjection() : camera.GetProjection();

    // Update all the frame constants and copy it in into the GPU ring buffer
    m_FrameConstants.mTime = m_ElapsedTime;
    m_FrameConstants.mDeltaTime = inDeltaTime;
    m_FrameConstants.mSunConeAngle = m_Settings.mSunConeAngle;
    m_FrameConstants.mFrameIndex = m_FrameCounter % sFrameCount;
    m_FrameConstants.mFrameCounter = m_FrameCounter;
    m_FrameConstants.mPrevJitter = m_FrameConstants.mJitter;
    m_FrameConstants.mJitter = enable_jitter ? Vec2(jitter_x, jitter_y) : Vec2(0.0f, 0.0f);
    m_FrameConstants.mSunColor = inScene->GetSunLight() ? inScene->GetSunLight()->GetColor() : Vec4(0.0f);
    m_FrameConstants.mSunDirection = Vec4(inScene->GetSunLightDirection(), 0.0f);
    m_FrameConstants.mCameraPosition = Vec4(camera.GetPosition(), 1.0f);
    m_FrameConstants.mViewportSize = inViewport.GetRenderSize(),
    m_FrameConstants.mViewMatrix = camera.GetView();
    m_FrameConstants.mInvViewMatrix = glm::inverse(camera.GetView());
    m_FrameConstants.mProjectionMatrix = final_proj_matrix;
    m_FrameConstants.mInvProjectionMatrix = glm::inverse(final_proj_matrix);
    m_FrameConstants.mPrevViewProjectionMatrix = m_FrameConstants.mViewProjectionMatrix;
    m_FrameConstants.mViewProjectionMatrix = final_proj_matrix * camera.GetView();
    m_FrameConstants.mInvViewProjectionMatrix = glm::inverse(final_proj_matrix * camera.GetView());

    if (m_FrameCounter == 0)
    {
        m_FrameConstants.mPrevJitter = m_FrameConstants.mJitter;
        m_FrameConstants.mPrevViewProjectionMatrix = m_FrameConstants.mViewProjectionMatrix;
    }

    // TODO: instead of updating this every frame, make these buffers global?
    {
        //m_FrameConstants.mDebugLinesVertexBuffer = inDevice.GetBindlessHeapIndex(m_RenderGraph.GetResources().GetBuffer(debug_lines_pass->GetData().mVertexBuffer));
        //m_FrameConstants.mDebugLinesIndirectArgsBuffer = inDevice.GetBindlessHeapIndex(m_RenderGraph.GetResources().GetBuffer(debug_lines_pass->GetData().mIndirectArgsBuffer));
    }

    // update RenderSettings
    RenderSettings::mActiveEntity = inApp->GetActiveEntity();

    if (inScene->Any<DDGISceneSettings>() && inScene->Count<DDGISceneSettings>())
    {
        const Entity& ddgi_entity = inScene->GetEntities<DDGISceneSettings>()[0];
        const Transform& ddgi_transform = inScene->Get<Transform>(ddgi_entity);
        const DDGISceneSettings& ddgi_settings = inScene->Get<DDGISceneSettings>(ddgi_entity);

        RenderSettings::mDDGIProbeCount = ddgi_settings.mDDGIProbeCount;
        RenderSettings::mDDGIProbeSpacing = ddgi_settings.mDDGIProbeSpacing;
        RenderSettings::mDDGICornerPosition = ddgi_transform.position;
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

    static const int& upload_tlas = g_CVariables->Create("upload_scene", 1, true);
    static const int& update_skinning = g_CVariables->Create("update_skinning", 1, true);
    // Start recording pending scene changes to the copy cmd list
    CommandList& copy_cmd_list = GetBackBufferData().mCopyCmdList;

    copy_cmd_list.Reset();

    // upload vertex buffers, index buffers, and BLAS for meshes
    {
        PIXScopedEvent(static_cast<ID3D12GraphicsCommandList*>( copy_cmd_list ), PIX_COLOR(0, 255, 0), "UPLOAD MESHES");

        for (Entity entity : m_PendingMeshUploads)
            inScene.UploadMesh(inApp, inDevice, inScene->Get<Mesh>(entity), inScene->GetPtr<Skeleton>(entity), copy_cmd_list);
    }
    
    // upload skeleton bone attributes and bone transform buffers
    for (Entity entity : m_PendingSkeletonUploads)
        inScene.UploadSkeleton(inApp, inDevice, inScene->Get<Skeleton>(entity), copy_cmd_list);
    
    // update bottom level AS for entities that have both a mesh and skeleton
    if (update_skinning)
    {
        PIXScopedEvent(static_cast<ID3D12GraphicsCommandList*>( copy_cmd_list ), PIX_COLOR(0, 255, 0), "BUILD BLASes");
        
        for (Entity entity : m_PendingBlasUpdates)
            inScene.UpdateBLAS(inApp, inDevice, inScene->Get<Mesh>(entity), inScene->GetPtr<Skeleton>(entity), copy_cmd_list);
    }

    // upload pixel data for texture assets 
    {
        PIXScopedEvent(static_cast<ID3D12GraphicsCommandList*>( copy_cmd_list ), PIX_COLOR(0, 255, 0), "UPLOAD TEXTURES");

        for (TextureUpload& texture_upload : m_PendingTextureUploads)
            inScene.UploadTexture(inApp, inDevice, texture_upload, copy_cmd_list);
    }

    // upload pixel data for textures that are associated with materials
    {
        PIXScopedEvent(static_cast<ID3D12GraphicsCommandList*>( copy_cmd_list ), PIX_COLOR(0, 255, 0), "UPLOAD MATERIALS");

        for (Entity entity : m_PendingMaterialUploads)
            inScene.UploadMaterial(inApp, inDevice, inScene->Get<Material>(entity), copy_cmd_list);
    }

    // clear all pending uploads for this frame, memory will be re-used
    m_PendingBlasUpdates.clear();
    m_PendingMeshUploads.clear();
    m_PendingTextureUploads.clear();
    m_PendingSkeletonUploads.clear();
    m_PendingMaterialUploads.clear();

    //// Record uploads for the RT scene 
    if (upload_tlas)
    {
        inScene.UploadInstances(inApp, inDevice, copy_cmd_list);
        inScene.UploadMaterials(inApp, inDevice, copy_cmd_list, m_Settings.mDisableAlbedo);
        inScene.UploadTLAS(inApp, inDevice, copy_cmd_list);
        inScene.UploadLights(inApp, inDevice, copy_cmd_list);
    }

    //// Submit all copy commands
    copy_cmd_list.Close();
    copy_cmd_list.Submit(inDevice, inDevice.GetGraphicsQueue());

    // Start recording graphics commands
    CommandList& direct_cmd_list = GetBackBufferData().mDirectCmdList;
    direct_cmd_list.Reset();

    // Record the entire frame into the direct cmd list
    m_RenderGraph.Execute(inDevice, direct_cmd_list);

    // Record commands to render ImGui to the backbuffer
    // skip if we recompiled, ImGui's descriptor tables will be invalid for 1 frame
    if (inApp->GetSettings().mShowUI && !recompiled)
        RenderImGui(m_RenderGraph, inDevice, direct_cmd_list, GetBackBufferData().mBackBuffer);

    direct_cmd_list.Close();

    // Run command list execution and present in a job so it can overlap a bit with the start of the next frame
    //m_PresentJobPtr = Async::sQueueJob([&inDevice, this]() 
    {
        PROFILE_SCOPE_CPU("IDXGISwapChain::Present");

        // submit all graphics commands 
        direct_cmd_list.Submit(inDevice, inDevice.GetGraphicsQueue());

        int sync_interval = m_Settings.mEnableVsync;
        uint32_t present_flags = 0u;

        if (!m_Settings.mEnableVsync && inDevice.IsTearingSupported())
            present_flags = DXGI_PRESENT_ALLOW_TEARING;

        gThrowIfFailed(m_Swapchain->Present(sync_interval, present_flags));

        GetBackBufferData().mFenceValue++;
        m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();
        gThrowIfFailed(inDevice.GetGraphicsQueue()->Signal(m_Fence.Get(), GetBackBufferData().mFenceValue));
    }
    // );

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

    const DefaultTexturesData& default_textures = AddDefaultTexturesPass(m_RenderGraph, inDevice, inRenderInterface->GetBlackTexture(), inRenderInterface->GetWhiteTexture());

    GBufferOutput gbuffer_output = GBufferOutput
    {
        .mDepthTexture = default_textures.mWhiteTexture,
        .mRenderTexture = default_textures.mBlackTexture,
        .mVelocityTexture = default_textures.mBlackTexture,
        .mSelectionTexture = default_textures.mBlackTexture
    };

    DDGIOutput ddgi_output = 
    {
        .mOutput = default_textures.mBlackTexture,
        .mDepthProbes = default_textures.mWhiteTexture,
        .mIrradianceProbes = default_textures.mBlackTexture,
    };

    RenderGraphResourceID compose_input = default_textures.mBlackTexture;

    RenderGraphResourceID ssr_texture = default_textures.mBlackTexture;
    RenderGraphResourceID ssao_texture = default_textures.mWhiteTexture;
    RenderGraphResourceID lighting_texture = default_textures.mBlackTexture;

    RenderGraphResourceID ao_texture = default_textures.mWhiteTexture;
    RenderGraphResourceID rt_shadows_texture = default_textures.mWhiteTexture;
    RenderGraphResourceID reflections_texture = default_textures.mBlackTexture;
    RenderGraphResourceID indirect_diffuse_texture = default_textures.mBlackTexture;

    const SkinningData& skinning_data = AddSkinningPass(m_RenderGraph, inDevice, inScene);

    const SkyCubeData& sky_cube_data = AddSkyCubePass(m_RenderGraph, inDevice, inScene);

    const ConvolveCubeData& convolved_cube_data = AddConvolveSkyCubePass(m_RenderGraph, inDevice, sky_cube_data);

    if (m_Settings.mDoPathTrace)
    {
        if (m_Settings.mDoPathTraceGBuffer)
            gbuffer_output = AddGBufferPass(m_RenderGraph, inDevice, inScene).mOutput;

        compose_input = AddPathTracePass(m_RenderGraph, inDevice, inScene, sky_cube_data).mOutputTexture;
    }
    else
    {
        gbuffer_output = AddGBufferPass(m_RenderGraph, inDevice, inScene).mOutput;

        // const auto& grass_data = AddGrassRenderPass(m_RenderGraph, inDevice, gbuffer_data);

        //const auto shadow_texture = AddShadowMaskPass(m_RenderGraph, inDevice, inScene, gbuffer_data).mOutputTexture;

        if (m_Settings.mEnableShadows)
            rt_shadows_texture = AddRayTracedShadowsPass(m_RenderGraph, inDevice, inScene, gbuffer_output);

        if (m_Settings.mEnableSSAO)
            ao_texture = AddSSAOTracePass(m_RenderGraph, inDevice, gbuffer_output).mOutputTexture;
        
        if (m_Settings.mEnableRTAO)
            ao_texture = AddAmbientOcclusionPass(m_RenderGraph, inDevice, inScene, gbuffer_output).mOutputTexture;

        if (m_Settings.mEnableReflections)
            reflections_texture = AddReflectionsPass(m_RenderGraph, inDevice, inScene, gbuffer_output, sky_cube_data).mOutputTexture;

        // const auto& downsample_data = AddDownsamplePass(m_RenderGraph, inDevice, reflection_data.mOutputTexture);

        if (m_Settings.mEnableDDGI)
            ddgi_output = AddDDGIPass(m_RenderGraph, inDevice, inScene, gbuffer_output, sky_cube_data);
        
        const TiledLightCullingData& light_cull_data = AddTiledLightCullingPass(m_RenderGraph, inDevice, inScene);

        const LightingData& light_data = AddLightingPass(m_RenderGraph, inDevice, inScene, gbuffer_output, light_cull_data, sky_cube_data.mSkyCubeTexture, rt_shadows_texture, reflections_texture, ao_texture, indirect_diffuse_texture);
        
        compose_input = light_data.mOutputTexture;
        
        if (m_Settings.mEnableDDGI && m_Settings.mDebugProbes)
            AddProbeDebugPass(m_RenderGraph, inDevice, ddgi_output, light_data.mOutputTexture, gbuffer_output.mDepthTexture);

        if (m_Settings.mEnableDDGI && m_Settings.mDebugProbeRays)
            AddProbeDebugRaysPass(m_RenderGraph, inDevice, light_data.mOutputTexture, gbuffer_output.mDepthTexture);

        if (m_Settings.mEnableTAA)
            compose_input = AddTAAResolvePass(m_RenderGraph, inDevice, gbuffer_output, light_data.mOutputTexture, m_FrameCounter).mOutputTexture;
        
        if (m_Settings.mEnableSSR)
            AddSSRTracePass(m_RenderGraph, inDevice, gbuffer_output, compose_input).mOutputTexture;
    }

    RenderGraphResourceID bloom_output = compose_input;

    const EDebugTexture debug_texture = EDebugTexture(inRenderInterface->GetSettings().mDebugTexture);
    
    // turn off any post processing effects for debug textures (this might change in the future)
    if (debug_texture == DEBUG_TEXTURE_NONE)
    {
         if (m_Settings.mEnableBloom)
            bloom_output = AddBloomPass(m_RenderGraph, inDevice, compose_input).mOutputTexture;

        if (m_Settings.mEnableDoF && gbuffer_output.mDepthTexture != default_textures.mWhiteTexture)
            compose_input = AddDepthOfFieldPass(m_RenderGraph, inDevice, compose_input, gbuffer_output.mDepthTexture).mOutputTexture;

        if (m_Settings.mEnableDebugOverlay && gbuffer_output.mDepthTexture != default_textures.mWhiteTexture)
            AddDebugOverlayPass(m_RenderGraph, inDevice, compose_input, gbuffer_output.mDepthTexture);
    }
    else
    {
        switch (debug_texture)
        { // all the debug textures that need tonemapping/gamma adjustment should go here, so they go through the compose pass
            case DEBUG_TEXTURE_RT_REFLECTIONS:
                compose_input = reflections_texture;
                break;
            case DEBUG_TEXTURE_RT_INDIRECT_DIFFUSE:
                compose_input = indirect_diffuse_texture;
                break;
        }

        bloom_output = compose_input;
    }

    if (!m_Settings.mEnableTAA)
    {
        switch (m_Upscaler.GetActiveUpscaler())
        {
            case UPSCALER_FSR:
                    compose_input = AddFsrPass(m_RenderGraph, inDevice, m_Upscaler, gbuffer_output, compose_input).mOutputTexture;
                    break;
            case UPSCALER_DLSS:
                    compose_input = AddDLSSPass(m_RenderGraph, inDevice, m_Upscaler, gbuffer_output, compose_input).mOutputTexture;
                    break;
            case UPSCALER_XESS:
                    compose_input = AddXeSSPass(m_RenderGraph, inDevice, m_Upscaler, gbuffer_output, compose_input).mOutputTexture;
                    break;
        }
    }

    const ComposeData& compose_data = AddComposePass(m_RenderGraph, inDevice, bloom_output, compose_input);

    RenderGraphResourceID final_output = compose_data.mOutputTexture;

    switch (debug_texture)
    {
        case DEBUG_TEXTURE_SSR:
            final_output = ssr_texture;
            break;
        case DEBUG_TEXTURE_SSAO:
            final_output = ssao_texture;
            break;
        case DEBUG_TEXTURE_LIGHTING:
            final_output = lighting_texture;
            break;
        case DEBUG_TEXTURE_RT_SHADOWS:
            final_output = rt_shadows_texture;
            break;
        case DEBUG_TEXTURE_RT_AMBIENT_OCCLUSION:
            final_output = ao_texture;
            break;
        case DEBUG_TEXTURE_GBUFFER_DEPTH:
        case DEBUG_TEXTURE_GBUFFER_ALBEDO:
        case DEBUG_TEXTURE_GBUFFER_NORMALS:
        case DEBUG_TEXTURE_GBUFFER_EMISSIVE:
        case DEBUG_TEXTURE_GBUFFER_VELOCITY:
        case DEBUG_TEXTURE_GBUFFER_METALLIC:
        case DEBUG_TEXTURE_GBUFFER_ROUGHNESS:
            final_output = AddGBufferDebugPass(m_RenderGraph, inDevice, gbuffer_output, debug_texture).mOutputTexture;
            break;
    }

    m_EntityTexture = gbuffer_output.mSelectionTexture;
    m_DisplayTexture = AddPreImGuiPass(m_RenderGraph, inDevice, final_output).mDisplayTextureSRV;

    // const auto& imgui_data = AddImGuiPass(m_RenderGraph, inDevice, inStagingHeap, compose_data.mOutputTexture);

    m_RenderGraph.Compile(inDevice, m_GlobalConstants);
}



void Renderer::WaitForIdle(Device& inDevice)
{
    if (m_PresentJobPtr)
        m_PresentJobPtr->WaitCPU();

    for (BackBufferData& backbuffer_data : m_BackBufferData)
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



TextureID Renderer::GetEntityTexture() const
{
    return m_RenderGraph.GetResources().GetTexture(m_EntityTexture);
}



TextureID Renderer::GetDisplayTexture() const
{
    return m_RenderGraph.GetResources().GetTextureView(m_DisplayTexture);
}



RenderInterface::RenderInterface(Application* inApp, Device& inDevice, Renderer& inRenderer, const RenderGraphResources& inResources) :
    m_Device(inDevice), m_Viewport(inApp->GetViewport()), m_Renderer(inRenderer), m_Resources(inResources)
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

    char ch = ' ';
    char description[128];
    WideCharToMultiByte(CP_ACP, 0, adapter_desc.Description, -1, description, 128, &ch, NULL);

    gpu_info.mProduct = std::string(description);
    gpu_info.mActiveAPI = "DirectX 12 Ultimate";

    SetGPUInfo(gpu_info);

    // Create default textures / assets
    const String light_texture_file = TextureAsset::Convert("Assets/light.png");
    if (!light_texture_file.empty())
    {
        m_LightTexture = TextureID(UploadTextureFromAsset(inApp->GetAssets()->GetAsset<TextureAsset>(light_texture_file)));
        m_Renderer.QueueTextureUpload(m_LightTexture, 0, inApp->GetAssets()->GetAsset<TextureAsset>(light_texture_file));
    }
}



void RenderInterface::UpdateGPUStats(Device& inDevice)
{
    m_GPUStats.mFrameCounter = m_Renderer.GetFrameCounter();
    m_GPUStats.mLiveBuffers.store(inDevice.GetBufferPool().GetSize());
    m_GPUStats.mLiveTextures.store(inDevice.GetTexturePool().GetSize());
    m_GPUStats.mLiveDSVHeap.store(inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV).GetSize());
    m_GPUStats.mLiveRTVHeap.store(inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV).GetSize());
    m_GPUStats.mLiveSamplerHeap.store(inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).GetSize());
    m_GPUStats.mLiveResourceHeap.store(inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).GetSize());
}



uint64_t RenderInterface::GetDisplayTexture()
{
    return m_Device.GetGPUDescriptorHandle(m_Renderer.GetDisplayTexture()).ptr;
}



uint64_t RenderInterface::GetLightTexture()
{
    return m_LightTexture.IsValid() ? m_Device.GetGPUDescriptorHandle(m_LightTexture).ptr : 0;
}



uint64_t RenderInterface::GetImGuiTextureID(uint32_t inHandle)
{
    uint32_t heap_index = m_Device.GetBindlessHeapIndex(TextureID(inHandle));
    uint32_t handle_size = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    DescriptorHeap& resource_heap = m_Device.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    uint64_t heap_ptr = resource_heap->GetGPUDescriptorHandleForHeapStart().ptr + heap_index * handle_size;
    
    return heap_ptr;
}



const char* RenderInterface::GetDebugTextureName(uint32_t inIndex) const
{
    constexpr std::array names =
    {
        "None",
        "Depth",
        "Albedo",
        "Normals",
        "Emissive",
        "Velocity",
        "Metallic",
        "Roughness",
        "Lighting",
        "SSR",
        "SSAO",
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
    const int indices_size = inMesh.indices.size() * sizeof(inMesh.indices[0]);
    const int vertices_size = inMesh.vertices.size() * sizeof(inMesh.vertices[0]);

    if (!vertices_size || !indices_size)
        return;

    inMesh.indexBuffer = m_Device.CreateBuffer(Buffer::Desc{
        .format = DXGI_FORMAT_R32_UINT,
        .size   = uint32_t(indices_size),
        .stride = sizeof(uint32_t) * 3,
        .usage  = Buffer::Usage::INDEX_BUFFER,
        .debugName = "IndexBuffer"
    }).GetValue();

    inMesh.vertexBuffer = m_Device.CreateBuffer(Buffer::Desc{
        .size   = uint32_t(vertices_size),
        .stride = sizeof(Vertex),
        .usage  = Buffer::Usage::VERTEX_BUFFER,
        .debugName = "VertexBuffer"
    }).GetValue();

    // actual data upload happens in RayTracedScene::UploadMesh at the start of the frame
    m_Renderer.QueueMeshUpload(inEntity);
}



void RenderInterface::UploadSkeletonBuffers(Entity inEntity, Skeleton& inSkeleton, Mesh& inMesh)
{
    inSkeleton.boneTransformMatrices.resize(inSkeleton.boneOffsetMatrices.size(), Mat4x4(1.0f));
    inSkeleton.boneWSTransformMatrices.resize(inSkeleton.boneOffsetMatrices.size(), Mat4x4(1.0f));

    inSkeleton.boneIndexBuffer = m_Device.CreateBuffer(Buffer::Desc {
        .size   = uint32_t(inSkeleton.boneIndices.size() * sizeof(IVec4)),
        .stride = sizeof(IVec4),
        .usage  = Buffer::Usage::SHADER_READ_ONLY,
        .debugName = "BoneIndicesBuffer"
    }).GetValue();

    inSkeleton.boneWeightBuffer = m_Device.CreateBuffer(Buffer::Desc {
        .size   = uint32_t(inSkeleton.boneWeights.size() * sizeof(Vec4)),
        .stride = sizeof(Vec4),
        .usage  = Buffer::Usage::SHADER_READ_ONLY,
        .debugName = "BoneWeightsBuffer"
    }).GetValue();

    inSkeleton.boneTransformsBuffer = m_Device.CreateBuffer(Buffer::Desc {
        .size   = uint32_t(inSkeleton.boneTransformMatrices.size() * sizeof(Mat4x4)),
        .stride = sizeof(Mat4x4),
        .usage  = Buffer::Usage::SHADER_READ_ONLY,
        .debugName = "BoneTransformsBuffer"
    }).GetValue();

    inSkeleton.skinnedVertexBuffer = m_Device.CreateBuffer(Buffer::Desc {
        .size   = uint32_t(sizeof(inMesh.vertices[0]) * inMesh.vertices.size()),
        .stride = sizeof(RTVertex),
        .usage  = Buffer::Usage::SHADER_READ_WRITE,
        .debugName = "SkinnedVertexBuffer"
    }).GetValue();

    m_Renderer.QueueSkeletonUpload(inEntity);
}



void RenderInterface::UploadMaterialTextures(Entity inEntity, Material& inMaterial, Assets& inAssets)
{
    IRenderInterface::UploadMaterialTextures(inEntity, inMaterial, inAssets);
    // UploadMaterialTextures only creates the gpu texture and srv, data upload happens in RayTracedScene::UploadMaterial at the start of the frame
    m_Renderer.QueueMaterialUpload(inEntity);
}


void RenderInterface::CompileMaterialShaders(Entity inEntity, Material& inMaterial)
{
    if (fs::exists(inMaterial.pixelShaderFile) && fs::is_regular_file(inMaterial.pixelShaderFile))
        g_ShaderCompiler.CompileShader(inMaterial.pixelShaderFile, SHADER_TYPE_PIXEL, "", inMaterial.pixelShader);

    if (fs::exists(inMaterial.vertexShaderFile) && fs::is_regular_file(inMaterial.vertexShaderFile))
        g_ShaderCompiler.CompileShader(inMaterial.vertexShaderFile, SHADER_TYPE_VERTEX, "", inMaterial.vertexShader);
}


void RenderInterface::ReleaseMaterialShaders(Entity inEntity, Material& inMaterial)
{
    g_ShaderCompiler.ReleaseShader(inMaterial.pixelShader);
    g_ShaderCompiler.ReleaseShader(inMaterial.vertexShader);
}


uint32_t RenderInterface::UploadTextureFromAsset(const TextureAsset::Ptr& inAsset, bool inIsSRGB, uint8_t inSwizzle)
{
    const uint8_t* data_ptr = inAsset->GetData();
    const DDS_HEADER* header_ptr = inAsset->GetHeader();

    const uint32_t mipmap_levels = header_ptr->dwMipMapCount;
    const String& debug_name = inAsset->GetPathAsString();

    DXGI_FORMAT format = inIsSRGB ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM;
    EDDSFormat dds_format = (EDDSFormat)header_ptr->ddspf.dwFourCC;

    switch (dds_format)
    {
        case EDDSFormat::DDS_FORMAT_ATI2: format = DXGI_FORMAT_BC5_UNORM; break;
        default: break;
    }

    if (dds_format == EDDSFormat::DDS_FORMAT_DX10)
        format = (DXGI_FORMAT)inAsset->GetHeaderDXT10()->dxgiFormat;

   /* if (format == DXGI_FORMAT_BC7_UNORM && inIsSRGB)
        format = DXGI_FORMAT_BC7_UNORM_SRGB;*/

    const TextureID texture = m_Device.CreateTexture(Texture::Desc
    {
        .swizzle    = inSwizzle,
        .format     = format,
        .width      = header_ptr->dwWidth,
        .height     = header_ptr->dwHeight,
        .mipLevels  = mipmap_levels,
        .usage      = Texture::SHADER_READ_ONLY,
        .debugName  = debug_name.c_str()
    });

    return texture.GetValue();
}



void RenderInterface::OnResize(const Viewport& inViewport)
{
    m_Renderer.SetShouldResize(true);
}



CommandList& Renderer::StartSingleSubmit()
{
    CommandList& cmd_list = GetBackBufferData().mDirectCmdList;
    cmd_list.Begin();
    return cmd_list;
}



void Renderer::FlushSingleSubmit(Device& inDevice, CommandList& inCmdList)
{
    inCmdList.Close();

    const std::array cmd_lists = { (ID3D12CommandList*)inCmdList };
    inDevice.GetGraphicsQueue()->ExecuteCommandLists(cmd_lists.size(), cmd_lists.data());

    BackBufferData& backbuffer_data = GetBackBufferData();
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

        need_recompile |= ImGui::Checkbox("Rasterize GBuffer", (bool*)&m_Renderer.GetSettings().mDoPathTraceGBuffer);

        ImGui::SameLine(0.0f, ImGui::CalcTextSize(" ").x);
        ImGui::Text("(?)");

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("GBuffer is needed for certain editor functionality.");

        if (ImGui::SliderInt("Bounces", (int*)&RenderSettings::mPathTraceBounces, 1, 8))
            RenderSettings::mPathTraceReset = true;

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
            constexpr const char* text = "...TAA is enabled";
            ImGui::SetCursorPosX(( ImGui::GetWindowWidth() - ImGui::CalcTextSize(text).x ) / 2.f);
            ImGui::TextDisabled(text);
        }
        else
        {
            constexpr std::array upscaler_items = { "No Upscaler", "AMD FSR2", "Nvidia DLSS", "Intel XeSS" };
            constexpr std::array upscaler_quality_items = { "Native", "Quality", "Balanced", "Performance" };

            Upscaler& upscaler = m_Renderer.GetUpscaler();

            if (ImGui::BeginCombo("##Upscaler", upscaler_items[upscaler.GetActiveUpscaler()], ImGuiComboFlags_None))
            {
                for (uint32_t upscaler_idx = 0; upscaler_idx < UPSCALER_COUNT; upscaler_idx++)
                {
                    if (ImGui::Selectable(upscaler_items[upscaler_idx], upscaler.GetActiveUpscaler() == upscaler_idx))
                    {
                        upscaler.SetActiveUpscaler(EUpscaler(upscaler_idx));
                        need_recompile = true;
                    }
                }

                ImGui::EndCombo();
            }

            if (upscaler.GetActiveUpscaler() != UPSCALER_NONE)
            {
                if (ImGui::BeginCombo("##UpscalerQuality", upscaler_quality_items[upscaler.GetActiveUpscalerQuality()], ImGuiComboFlags_None))
                {
                    for (uint32_t quality_idx = 0; quality_idx < UPSCALER_QUALITY_COUNT; quality_idx++)
                    {
                        if (ImGui::Selectable(upscaler_quality_items[quality_idx], upscaler.GetActiveUpscalerQuality() == EUpscalerQuality(quality_idx)))
                        {
                            upscaler.SetActiveUpscalerQuality(EUpscalerQuality(quality_idx));
                            need_recompile = true;
                        }
                    }

                    ImGui::EndCombo();
                }
            }
        }

        ImGui::EndMenu();
    }

    need_recompile |= ImGui::Checkbox("Enable Debug Overlay", (bool*)&m_Renderer.GetSettings().mEnableDebugOverlay);
    
    ImGui::AlignTextToFramePadding();

    if (ImGui::BeginMenu("Debug Settings"))
    {
        ImGui::SeparatorText("Debug");

        if (ImGui::Button("PIX GPU Capture"))
            m_Renderer.SetShouldCaptureNextFrame(true);

        if (ImGui::Button("Lower Dynamic Lights"))
        {
            for (const auto& [entity, light] : inScene.Each<Light>())
            {
                if (light.type == LIGHT_TYPE_SPOT || light.type == LIGHT_TYPE_POINT)
                {
                    light.colour.a /= 1000.0f;
                }
            }
        }

        if (ImGui::Button("Re-calculate Normals"))
        {
            for (const auto& [entity, mesh] : inScene.Each<Mesh>())
            {
                mesh.CalculateNormals();
                mesh.CalculateTangents();
                mesh.CalculateVertices();
            }
        }

        if (ImGui::Button("Material albedo 1"))
        {
            for (const auto& [entity, material] : inScene.Each<Material>())
                material.albedo = Vec4(1.0f);
        }

        if (ImGui::Button("Flip mesh uvs Y"))
        {
            for (const auto& [entity, mesh] : inScene.Each<Mesh>())
            {
                for (Vec2& uv : mesh.uvs)
                    uv.y = 1.0 - uv.y;

                mesh.CalculateVertices();

                UploadMeshBuffers(entity, mesh);
            }
        }

        if (ImGui::Button("BC7_UNORM to BC7_UNORM_SRGB"))
        {
            for (const auto& [entity, material] : inScene.Each<Material>())
            {
                if (TextureAsset::Ptr texture = inApp->GetAssets()->GetAsset<TextureAsset>(material.albedoFile))
                {
                    if (texture->IsExtendedDX10())
                    {
                        if (texture->GetHeaderDXT10()->dxgiFormat == DXGI_FORMAT_BC7_UNORM)
                        {
                            texture->GetHeaderDXT10()->dxgiFormat = DXGI_FORMAT_BC7_UNORM_SRGB;

                            if  (texture->Save())
                                inApp->LogMessage(std::format("Saved {}", material.albedoFile));
                        }
                    }
                }
            }
        }

        if (ImGui::Button("Clear DDGI History"))
        {
        }

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
                Array<meshopt_Meshlet> meshlets(max_meshlets);
                Array<unsigned int> meshlet_vertices(max_meshlets * max_vertices);
                Array<unsigned char> meshlet_triangles(max_meshlets * max_triangles * 3);

                size_t meshlet_count = meshopt_buildMeshlets(meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(), mesh.indices.data(),
                    mesh.indices.size(), &mesh.positions[0].x, mesh.positions.size(), sizeof(mesh.positions[0]), max_vertices, max_triangles, cone_weight);

                meshlets.resize(meshlet_count);
                meshlet_vertices.resize(meshlet_count * max_vertices);
                meshlet_triangles.resize(meshlet_count * max_triangles * 3);

                mesh.meshlets.reserve(meshlet_count);
                mesh.meshletIndices.resize(meshlet_count * max_vertices);
                mesh.meshletTriangles.resize(meshlet_count * max_triangles);

                for (const meshopt_Meshlet& opt_meshlet : meshlets)
                {
                    Meshlet& meshlet = mesh.meshlets.emplace_back();
                    meshlet.mTriangleCount = opt_meshlet.triangle_count;
                    meshlet.mTriangleOffset = opt_meshlet.triangle_offset;
                    meshlet.mVertexCount = opt_meshlet.vertex_count;
                    meshlet.mVertexOffset = opt_meshlet.vertex_offset;
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
            const String file_path = OS::sSaveFileDialog("DOT File (*.dot)\0", "dot");

            if (!file_path.empty())
            {
                std::ofstream ofs = std::ofstream(file_path);
                ofs << m_Renderer.GetRenderGraph().ToGraphVizText(m_Device, TextureID());
            }
        }

        ImGui::EndMenu();
    }

    ImGui::AlignTextToFramePadding();


    if (ImGui::BeginMenu("Window Settings"))
    {
        ImGui::SeparatorText("Window");

        static constexpr std::array modes =  
        { 
            0u /* Windowed */, 
            (uint32_t)SDL_WINDOW_FULLSCREEN_DESKTOP, 
            (uint32_t)SDL_WINDOW_FULLSCREEN 
        };

        static constexpr std::array mode_strings =
        {
            "Windowed", 
            "Borderless", 
            "Fullscreen" 
        };

        int mode = 0u; // Windowed
        SDL_Window* window = inApp->GetWindow();

        if (inApp->IsWindowBorderless())
            mode = 1u;
        else if (inApp->IsWindowExclusiveFullscreen())
            mode = 2u;

        if (ImGui::BeginCombo("Mode", mode_strings[mode]))
        {
            for (const auto& [index, string] : gEnumerate(mode_strings))
            {
                if (ImGui::Selectable(string, index == mode))
                {
                    SDL_SetWindowFullscreen(window, modes[index]);

                    if (modes[index] == SDL_WINDOW_FULLSCREEN)
                    {
                        while (!inApp->IsWindowExclusiveFullscreen())
                            SDL_Delay(10);
                    }

                    m_Renderer.SetShouldResize(true);
                }
            }

            ImGui::EndCombo();
        }

        Array<String> display_names(SDL_GetNumVideoDisplays());
        for (const auto& [index, name] : gEnumerate(display_names))
            name = SDL_GetDisplayName(index);

        const int display_index = SDL_GetWindowDisplayIndex(window);
        if (ImGui::BeginCombo("Monitor", display_names[display_index].c_str()))
        {
            for (const auto& [index, name] : gEnumerate(display_names))
            {
                if (ImGui::Selectable(name.c_str(), index == display_index))
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

        const int current_display_index = SDL_GetWindowDisplayIndex(window);
        const int num_display_modes = SDL_GetNumDisplayModes(current_display_index);

        Array<String> display_mode_strings(num_display_modes);
        Array<SDL_DisplayMode> display_modes(num_display_modes);

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
                    if (ImGui::Selectable(display_mode_strings[index].c_str(), index == m_Renderer.GetSettings().mDisplayRes))
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

        ImGui::InputInt("Target FPS", &m_Renderer.GetSettings().mTargetFps);

        ImGui::EndMenu();
    }

    if (0) {
        ImGui::Text("Grass Settings");
        ImGui::DragFloat("Grass Bend", &RenderSettings::mGrassBend, 0.01f, -1.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Grass Tilt", &RenderSettings::mGrassTilt, 0.01f, -1.0f, 1.0f, "%.2f");
        ImGui::DragFloat2("Wind Direction", &RenderSettings::mWindDirection[0], 0.01f, -10.0f, 10.0f, "%.1f");
        ImGui::NewLine();
    }

    ImGui::AlignTextToFramePadding();

    if (ImGui::BeginMenu("Ray Tracing Settings"))
    {
        //ImGui::SeparatorText("Settings");
        ImGui::SeparatorText("Ray Tracing");

        need_recompile |= ImGui::Checkbox("##Shadowstoggle", (bool*)&m_Renderer.GetSettings().mEnableShadows);

        ImGui::SameLine();

        if (ImGui::BeginMenu("Enable Shadows"))
        {
            ImGui::SeparatorText("Settings");

            ImGui::DragFloat("Sun Cone Angle", &m_Renderer.GetSettings().mSunConeAngle, 0.01f, 0.0f, 1.0f, "%.2f");

            ImGui::EndMenu();
        }

        need_recompile |= ImGui::Checkbox("Enable Reflections", (bool*)&m_Renderer.GetSettings().mEnableReflections);

        need_recompile |= ImGui::Checkbox("##GItoggle", (bool*)&m_Renderer.GetSettings().mEnableDDGI);
        ImGui::SameLine();

        if (ImGui::BeginMenu("Enable Indirect Diffuse"))
        {
            ImGui::SeparatorText("Debug Options");

            ImGui::Checkbox("Visualize Pure White Mode", (bool*)&m_Renderer.GetSettings().mDisableAlbedo);

            // TODO FIX DEBUG PROBE RAYS
            //need_recompile |= ImGui::Checkbox("Visualize Indirect Diffuse Rays", (bool*)&m_Renderer.GetSettings().mDebugProbeRays);

            need_recompile |= ImGui::Checkbox("##ddgiprobedebug", (bool*)&m_Renderer.GetSettings().mDebugProbes);
            
            ImGui::SameLine();

            if (ImGui::BeginMenu("Visualize Indirect Diffuse Probes"))
            {
                ImGui::SeparatorText("Settings");

                ImGui::DragFloat("Probe Radius", &RenderSettings::mDDGIDebugRadius, 0.01f, 0.01f, 10.0f, "%.2f");

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        need_recompile |= ImGui::Checkbox("##AOtoggle", (bool*)&m_Renderer.GetSettings().mEnableRTAO);
        ImGui::SameLine();

        if (ImGui::BeginMenu("Enable Ambient Occlusion"))
        {
            ImGui::SeparatorText("Settings");

            ImGui::DragFloat("Radius", &RenderSettings::mRTAORadius, 0.01f, 0.0f, 20.0f, "%.2f");
            ImGui::DragFloat("Intensity", &RenderSettings::mRTAOPower, 0.01f, 0.0f, 10.0f, "%.2f");
            ImGui::DragFloat("Normal Bias", &RenderSettings::mRTAONormalBias, 0.001f, 0.0f, 1.0f, "%.3f");
            ImGui::SliderInt("Sample Count", (int*)&RenderSettings::mRTAOSampleCount, 1u, 128u);
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    ImGui::AlignTextToFramePadding();

    ImGui::PushItemFlag(ImGuiItemFlags_NoWindowHoverableCheck | ImGuiItemFlags_AllowOverlap, true);

    if (ImGui::BeginMenu("Post Process Settings"))
    {
        ImGui::SeparatorText("Post Processing");

        need_recompile |= ImGui::Checkbox("##SSAOtoggle", (bool*)&m_Renderer.GetSettings().mEnableSSAO);
        ImGui::SameLine();

        if (ImGui::BeginMenu("SSAO"))
        {
            ImGui::SeparatorText("Settings");

            ImGui::DragFloat("Bias", &RenderSettings::mSSAOBias, 0.001f, 0.0f, 0.5f, "%.3f");
            ImGui::DragFloat("Radius", &RenderSettings::mSSAORadius, 0.01f, 0.0f, 0.5f, "%.2f");
            ImGui::DragInt("Samples", &RenderSettings::mSSAOSamples, 1, 1, 64);

            ImGui::EndMenu();
        }

        need_recompile |= ImGui::Checkbox("##Bloomtoggle", (bool*)&m_Renderer.GetSettings().mEnableBloom);
        ImGui::SameLine();

        if (ImGui::BeginMenu("Bloom"))
        {
            ImGui::SeparatorText("Settings");

            ImGui::DragFloat("Blend Factor", &RenderSettings::mBloomBlendFactor, 0.01f, 0.0f, 0.5f, "%.2f");

            ImGui::EndMenu();
        }

        need_recompile |= ImGui::Checkbox("##Vignettetoggle", (bool*)&m_Renderer.GetSettings().mEnableVignette);
        ImGui::SameLine();

        if (ImGui::BeginMenu("Vignette"))
        {
            ImGui::SeparatorText("Settings");

            ImGui::DragFloat("Scale", &RenderSettings::mVignetteScale, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat("Bias", &RenderSettings::mVignetteBias, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat("Inner Radius", &RenderSettings::mVignetteInner, 0.01f, 0.0f, RenderSettings::mVignetteOuter, "%.2f");
            ImGui::DragFloat("Outer Radius", &RenderSettings::mVignetteOuter, 0.01f, RenderSettings::mVignetteInner, 2.0f, "%.2f");

            ImGui::EndMenu();
        }

        need_recompile |= ImGui::Checkbox("##DOFtoggle", (bool*)&m_Renderer.GetSettings().mEnableDoF);
        ImGui::SameLine();


        if (ImGui::BeginMenu("Depth of Field"))
        {
            ImGui::SeparatorText("Settings");

            const float far_plane = inViewport.GetCamera().GetFar();
            const float near_plane = inViewport.GetCamera().GetNear();

            ImGui::DragFloat("Focus Scale", &RenderSettings::mDoFFocusScale, 0.01f, 0.0f, 4.0f, "%.2f");
            ImGui::DragFloat("Focus Point", &RenderSettings::mDoFFocusPoint, 0.01f, near_plane, far_plane, "%.2f");
            ImGui::EndMenu();
        }

        need_recompile |= ImGui::Checkbox("Auto Exposure", (bool*)&m_Renderer.GetSettings().mEnableAutoExposure);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Currently not implemented.");

        ImGui::DragFloat("Manual Exposure", &RenderSettings::mExposure, 0.01f, 0.0f, 100.0f, "%.2f");
        ImGui::DragFloat("Chromatic Aberration", &RenderSettings::mChromaticAberrationStrength, 0.01f, 0.0f, 10.0f, "%.2f");

        ImGui::EndMenu();
    }

    ImGui::PopItemFlag();

    if (need_recompile)
        m_Renderer.SetShouldRecompile(true); // call for a resize so the rendergraph gets recompiled (hacky, TODO: FIXME: pls fix)
}



uint32_t RenderInterface::GetSelectedEntity(const Scene& inScene, uint32_t inScreenPosX, uint32_t inScreenPosY)
{
    // ViewportWidget pre-flips the Y coordinate for OpenGL, undo that
    inScreenPosY = m_Viewport.GetRenderSize().y - inScreenPosY;

    CommandList cmd_list = CommandList(m_Device, D3D12_COMMAND_LIST_TYPE_DIRECT, 0);
    cmd_list.Reset();

    Texture& entity_texture = m_Device.GetTexture(m_Renderer.GetEntityTexture());
    ID3D12Resource* entity_texture_resource = entity_texture.GetD3D12Resource().Get();

    BufferID readback_buffer_id = m_Device.CreateBuffer(Buffer::Describe(sizeof(Entity), Buffer::READBACK, true, "PixelReadbackBuffer"));

    auto entity_texture_barrier = D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(entity_texture_resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE));
    cmd_list->ResourceBarrier(1, &entity_texture_barrier);
    
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    footprint.Footprint = CD3DX12_SUBRESOURCE_FOOTPRINT(entity_texture.GetDesc().format, 1, 1, 1, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

    const CD3DX12_BOX box = CD3DX12_BOX(inScreenPosX, inScreenPosY, inScreenPosX + 1, inScreenPosY + 1);
    const CD3DX12_TEXTURE_COPY_LOCATION src = CD3DX12_TEXTURE_COPY_LOCATION(entity_texture_resource, 0);
    const CD3DX12_TEXTURE_COPY_LOCATION dest = CD3DX12_TEXTURE_COPY_LOCATION(m_Device.GetD3D12Resource(readback_buffer_id), footprint);

    cmd_list->CopyTextureRegion(&dest, 0, 0, 0, &src, &box);

    std::swap(entity_texture_barrier.Transition.StateBefore, entity_texture_barrier.Transition.StateAfter);
    cmd_list->ResourceBarrier(1, &entity_texture_barrier);

    cmd_list.Close();

    cmd_list.Submit(m_Device, m_Device.GetGraphicsQueue());

    ComPtr<ID3D12Fence> fence = nullptr;
    m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

    HANDLE fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    m_Device.GetGraphicsQueue()->Signal(fence.Get(), 1);

    gThrowIfFailed(fence->SetEventOnCompletion(1, fence_event));
    WaitForSingleObjectEx(fence_event, INFINITE, FALSE);

    uint32_t* mapped_ptr = nullptr;
    const CD3DX12_RANGE range = CD3DX12_RANGE(0, 0);
    gThrowIfFailed(m_Device.GetBuffer(readback_buffer_id)->Map(0, &range, reinterpret_cast<void**>( &mapped_ptr )));

    Entity result = Entity(*mapped_ptr);

    m_Device.ReleaseBuffer(readback_buffer_id);

    return result;
}



TextureID InitImGui(Device& inDevice, DXGI_FORMAT inRtvFormat, uint32_t inFrameCount)
{
    int width = 0, height = 0;
    unsigned char* pixels = nullptr;
    ImGui::GetIO().Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

    TextureID font_texture_id = inDevice.CreateTexture(Texture::Desc
    {
        .format = DXGI_FORMAT_R8_UNORM,
        .width  = uint32_t(width),
        .height = uint32_t(height),
        .usage  = Texture::SHADER_READ_ONLY,
        .debugName = "ImGuiFontTexture"
    });

    DescriptorID font_texture_view = inDevice.GetTexture(font_texture_id).GetView();
    DescriptorHeap& descriptor_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

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
    inCmdList.BindDefaults(inDevice);

    {   // manual barriers around the imported backbuffer resource, the rendergraph doesn't handle this kind of state
        auto backbuffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetD3D12Resource(inBackBuffer), D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
        inCmdList->ResourceBarrier(1, &backbuffer_barrier);
    }

    const D3D12_VIEWPORT bb_viewport = CD3DX12_VIEWPORT(inDevice.GetTexture(inBackBuffer).GetD3D12Resource().Get());
    const D3D12_RECT bb_scissor = CD3DX12_RECT(bb_viewport.TopLeftX, bb_viewport.TopLeftY, bb_viewport.Width, bb_viewport.Height);

    inCmdList->RSSetViewports(1, &bb_viewport);
    inCmdList->RSSetScissorRects(1, &bb_scissor);

    const DescriptorHeap& rtv_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    const std::array rtv = { rtv_heap.GetCPUDescriptorHandle(inDevice.GetTexture(inBackBuffer).GetView()) };

    inCmdList->OMSetRenderTargets(rtv.size(), rtv.data(), FALSE, nullptr);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), inCmdList);

    {
        auto backbuffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetD3D12Resource(inBackBuffer), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON);
        inCmdList->ResourceBarrier(1, &backbuffer_barrier);
    }
}


} // raekor
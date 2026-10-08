#include "PCH.h"
#include "Renderer.h"

#include "Shared.h"
#include "Scene.h"
#include "Shader.h"
#include "Upscalers.h"
#include "RayTracing.h"
#include "GPUProfiler.h"
#include "RenderGraph.h"
#include "RenderPasses.h"

#include "OS.h"
#include "Hash.h"
#include "Iter.h"
#include "Timer.h"
#include "Profiler.h"
#include "Primitives.h"
#include "UIRenderer.h"
#include "Application.h"

namespace RK::DX12 {

Renderer::Renderer(Device& inDevice, const Viewport& inViewport, SDL_Window* inWindow) :
    m_Window(inWindow),
    m_RenderGraph(inDevice, inViewport, sFrameCount)
{
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc =
    {
        .Width = inViewport.GetDisplaySize().x,
        .Height = inViewport.GetDisplaySize().y,
        .Format = sSwapchainFormat,
        .SampleDesc = DXGI_SAMPLE_DESC {.Count = 1 },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = sFrameCount,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
    };

    if (inDevice.IsTearingSupported())
        swapchain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    ComPtr<IDXGIFactory4> factory = nullptr;
    gThrowIfFailed(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory)));

    SDL_PropertiesID props = SDL_GetWindowProperties(inWindow);
    HWND hwnd = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

    ComPtr<IDXGISwapChain1> swapchain = nullptr;
    gThrowIfFailed(factory->CreateSwapChainForHwnd(inDevice.GetGraphicsQueue(), hwnd, &swapchain_desc, nullptr, nullptr, &swapchain));
    gThrowIfFailed(swapchain.As(&m_Swapchain));

    // Disables DXGI's automatic ALT+ENTER and PRINTSCREEN
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_WINDOW_CHANGES);

    m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

    for (const auto& [index, backbuffer_data] : gEnumerate(m_BackBufferData))
    {
        ID3D12Resource* rtv_resource = nullptr;
        gThrowIfFailed(m_Swapchain->GetBuffer(index, IID_PPV_ARGS(&rtv_resource)));

        D3D12_RESOURCE_DESC rtv_resource_desc = rtv_resource->GetDesc();

        backbuffer_data.mBackBuffer = inDevice.CreateTextureView(rtv_resource, Texture::Desc
            {
                .format = rtv_resource_desc.Format,
                .width = uint32_t(rtv_resource_desc.Width),
                .height = uint32_t(rtv_resource_desc.Height),
                .usage = Texture::Usage::RENDER_TARGET,
                .debugName = "BackBuffer"
            });

        backbuffer_data.mDirectCmdList = CommandList(inDevice, D3D12_COMMAND_LIST_TYPE_DIRECT, index);
        backbuffer_data.mDirectCmdList->SetName(L"RK::DX12::CommandList(DIRECT)");

        backbuffer_data.mCopyCmdList = CommandList(inDevice, D3D12_COMMAND_LIST_TYPE_DIRECT, index);
        backbuffer_data.mCopyCmdList->SetName(L"RK::DX12::CommandList(COPY)");

        backbuffer_data.mUpdateCmdList = CommandList(inDevice, D3D12_COMMAND_LIST_TYPE_DIRECT, index);
        backbuffer_data.mUpdateCmdList->SetName(L"RK::DX12::CommandList(DIRECT)");

        rtv_resource->SetName(L"BACKBUFFER");
    }

    inDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));
    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (!m_FenceEvent)
        gThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));

    g_CVariables->CreateFn("fn_hotload_shaders", [this, &inDevice]()
    {
        WaitForIdle(inDevice);

        if (g_SystemShaders.OnHotLoad(inDevice))
        {
            inDevice.ClearPipelineCache();
            SetShouldRecompile(true);
        }
    });

    m_DebugLinesVertexBuffer = inDevice.CreateBuffer(Buffer::RWStructuredBuffer(sizeof(Vec4) * UINT16_MAX, sizeof(Vec4), "DebugLinesVertexBuffer"));
    m_DebugLinesIndirectArgsBuffer = inDevice.CreateBuffer(Buffer::RWByteAddressBuffer(sizeof(D3D12_DRAW_ARGUMENTS), "DebugLinesIndirectArgsBuffer"));
}



void Renderer::CreateDisplayTextureProxy(Device& inDevice)
{
    const D3D12_SHADER_RESOURCE_VIEW_DESC display_texture_proxy_desc =
    {
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D = D3D12_TEX2D_SRV { .MipLevels = 1 }
    };

    m_DisplayTextureProxy = inDevice.CreateShaderResourceView(nullptr, &display_texture_proxy_desc);
}



void Renderer::CreateProbeDebugMesh(Device& inDevice)
{
    Mesh::CreateSphere(m_ProbeDebugMesh, 0.5f, 32u, 32u);

    const uint64_t indices_size = m_ProbeDebugMesh.indices.size() * sizeof(m_ProbeDebugMesh.indices[0]);
    const uint64_t vertices_size = m_ProbeDebugMesh.vertices.size() * sizeof(m_ProbeDebugMesh.vertices[0]);

    m_ProbeDebugMesh.indexBuffer = inDevice.CreateBuffer(Buffer::Desc
    {
        .size   = uint32_t(indices_size),
        .stride = sizeof(uint32_t) * 3,
        .usage  = Buffer::Usage::INDEX_BUFFER,
        .mappable = true,
        .debugName = "DebugProbeIndices"
    }).GetValue();

    m_ProbeDebugMesh.vertexBuffer = inDevice.CreateBuffer(Buffer::Desc
    {
        .size   = uint32_t(vertices_size),
        .stride = sizeof(Vertex),
        .usage  = Buffer::Usage::VERTEX_BUFFER,
        .mappable = true,
        .debugName = "DebugProbeVertices"
    }).GetValue();

    {
        Buffer& index_buffer = inDevice.GetBuffer(BufferID(m_ProbeDebugMesh.indexBuffer));
        void* mapped_ptr = nullptr;
        index_buffer->Map(0, nullptr, &mapped_ptr);
        memcpy(mapped_ptr, m_ProbeDebugMesh.indices.data(), indices_size);
        index_buffer->Unmap(0, nullptr);
    }

    {
        Buffer& vertex_buffer = inDevice.GetBuffer(BufferID(m_ProbeDebugMesh.vertexBuffer));
        void* mapped_ptr = nullptr;
        vertex_buffer->Map(0, nullptr, &mapped_ptr);
        memcpy(mapped_ptr, m_ProbeDebugMesh.vertices.data(), vertices_size);
        vertex_buffer->Unmap(0, nullptr);
    }
}



void Renderer::UpdateFontAtlas(Device& inDevice)
{
    if (!g_UIRenderer.HasFont() || g_UIRenderer.GetFontAtlasVersion() == m_FontAtlasVersion)
        return;

    if (m_FontAtlasTexture.IsValid())
        inDevice.ReleaseTexture(m_FontAtlasTexture);

    m_FontAtlasTexture = inDevice.CreateTexture(Texture::Desc
    {
        .format = DXGI_FORMAT_R8_UNORM,
        .width  = g_UIRenderer.GetFontAtlasWidth(),
        .height = g_UIRenderer.GetFontAtlasHeight(),
        .usage  = Texture::Usage::SHADER_READ_ONLY,
        .debugName = "UIFontAtlas"
    });

    inDevice.UploadTextureData(inDevice.GetTexture(m_FontAtlasTexture), 0, 0, g_UIRenderer.GetFontAtlasWidth(), g_UIRenderer.GetFontAtlas().data());

    m_FontAtlasVersion = g_UIRenderer.GetFontAtlasVersion();
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
        ID3D12Resource* rtv_resource = nullptr;
        gThrowIfFailed(m_Swapchain->GetBuffer(index, IID_PPV_ARGS(&rtv_resource)));

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
                .width = uint32_t(rtv_resource_desc.Width),
                .height = uint32_t(rtv_resource_desc.Height),
                .usage = Texture::Usage::RENDER_TARGET,
                .debugName = swapchain_buffer_names[index]
            });
    }

    m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

    m_Settings.mFullscreen = inFullScreen;
}



void Renderer::OnResizeViewport(Device& inDevice, Viewport& inViewport)
{
    PROFILE_FUNCTION_CPU();

    if (m_Upscaler.GetActiveUpscaler() != UPSCALER_NONE)
        WaitForIdle(inDevice);

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

    m_ViewportKey = GetViewportKey(inViewport);
}



void Renderer::OnRender(Application* inApp, Device& inDevice, Viewport& inViewport, const Scene& inScene, float inDeltaTime)
{
    PROFILE_FUNCTION_CPU();

    // Check if any of the shader sources were updated and recompile them if necessary.
    // the OS file stamp checks are expensive so we only turn this on in debug builds.
    static bool force_hotload = OS::sCheckCommandLineOption("-force_enable_hotload");
    const bool shaders_hotloaded = IF_DEBUG_ELSE(g_SystemShaders.OnHotLoad(inDevice), force_hotload ? g_SystemShaders.OnHotLoad(inDevice) : false);
    if (shaders_hotloaded)
        gLogInfo("DX12", "Hotloaded system shaders");

    static bool do_stress_test = OS::sCheckCommandLineOption("-stress_test");

    m_RenderWorld.Extract(inScene, inDevice, RenderSettings::GetExposure(), m_Settings.mDisableAlbedo);

    if (const RenderDDGIVolume& ddgi_volume = m_RenderWorld.GetDDGIVolume(); ddgi_volume.mEnabled)
    {
        RenderSettings::mDDGIProbeCount = glm::max(ddgi_volume.mProbeCount, IVec3(1));
        RenderSettings::mDDGIProbeSpacing = ddgi_volume.mProbeSpacing;
        RenderSettings::mDDGICornerPosition = ddgi_volume.mCornerPosition;
        RenderSettings::mDDGIFollowCamera = ddgi_volume.mFollowCamera;
        RenderSettings::mDDGICascadeCount = glm::clamp(ddgi_volume.mCascadeCount, 1, DDGI_MAX_CASCADES);
    }

    RenderSettings::UpdateDDGIVolumes(inViewport.GetPosition());

    const bool resize_viewport = m_ShouldResize || GetViewportKey(inViewport) != m_ViewportKey;
    const bool need_recompile = resize_viewport || shaders_hotloaded || m_ShouldRecompile || GetRenderGraphKey() != m_RenderGraphKey || ( do_stress_test && m_FrameCounter > 60 );

    if (need_recompile)
    {
        Timer timer;

        if (m_ShouldResize || shaders_hotloaded)
        {
            WaitForIdle(inDevice);

            if (shaders_hotloaded)
                inDevice.ClearPipelineCache();

            if (m_ShouldResize)
                OnResize(inDevice, inViewport, inApp->IsWindowExclusiveFullscreen());
        }

        if (resize_viewport)
            OnResizeViewport(inDevice, inViewport);

        Recompile(inDevice);

        m_FrameCounter = 0;
        m_ShouldResize = false;
        m_ShouldRecompile = false;

        gLogDebug("Renderer", "Recompiled the render graph in {:.3f} ms", Timer::sToMilliseconds(timer.GetElapsedTime()));
    }

    // At this point in the frame we really need the previous frame's present job to have finished
    if (m_PresentJobPtr)
        m_PresentJobPtr->Wait();

    BackBufferData& backbuffer_data = GetBackBufferData();
    uint64_t completed_value = m_Fence->GetCompletedValue();

    // make sure the backbuffer data we're about to use is no longer being used by the GPU
    if (completed_value < backbuffer_data.mFenceValue)
    {
        gThrowIfFailed(m_Fence->SetEventOnCompletion(backbuffer_data.mFenceValue, m_FenceEvent));
        WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
    }

    // at this point we know the GPU is no longer working on this frame, so free/release stuff here
    if (backbuffer_data.mFenceValue > 0)
    {
        g_GPUProfiler->Readback(inDevice, m_FrameIndex);

        inDevice.RetireUploadBuffers(backbuffer_data.mCopyCmdList);
        inDevice.RetireUploadBuffers(backbuffer_data.mDirectCmdList);
        inDevice.RetireUploadBuffers(backbuffer_data.mUpdateCmdList);
    }

    ResolveReadbacks(inDevice);


    // Update the total running time of the application / renderer
    m_ElapsedTime += inDeltaTime;

    // Calculate a jittered projection matrix for TAA/FSR/DLSS/XESS
    const Viewport& vp = m_RenderGraph.GetViewport();
    const int32_t jitter_phase_count = ffxFsr2GetJitterPhaseCount(m_RenderGraph.GetViewport().GetRenderSize().x, m_RenderGraph.GetViewport().GetDisplaySize().x);

    float jitter_offset_x = 0;
    float jitter_offset_y = 0;
    ffxFsr2GetJitterOffset(&jitter_offset_x, &jitter_offset_y, m_FrameCounter, jitter_phase_count);

    const float jitter_x = 2.0f * jitter_offset_x / (float)m_RenderGraph.GetViewport().GetRenderSize().x * m_Settings.mJitterScale;
    const float jitter_y = -2.0f * jitter_offset_y / (float)m_RenderGraph.GetViewport().GetRenderSize().y * m_Settings.mJitterScale;
    const Mat4x4 jitter_matrix = glm::translate(Mat4x4(1.0f), Vec3(jitter_x, jitter_y, 0));

    bool enable_jitter = m_Settings.mEnableTAA || m_Upscaler.GetActiveUpscaler();
    enable_jitter &= m_Settings.mDebugTexture == DEBUG_TEXTURE_NONE;
    enable_jitter &= !m_Settings.mDoPathTrace;

    const Mat4x4 final_proj_matrix = enable_jitter ? jitter_matrix * vp.GetProjection() : vp.GetProjection();

    // Update all the frame constants and copy it in into the GPU ring buffer
    m_FrameConstants.mTime = m_ElapsedTime;
    m_FrameConstants.mDeltaTime = inDeltaTime;
    m_FrameConstants.mExposure = RenderSettings::GetExposure();
    m_FrameConstants.mSunConeAngle = m_Settings.mSunConeAngle;
    m_FrameConstants.mFrameCounter = m_FrameCounter;
    m_FrameConstants.mPrevJitter = m_FrameConstants.mJitter;
    m_FrameConstants.mJitter = enable_jitter ? Vec2(jitter_x, jitter_y) : Vec2(0.0f, 0.0f);
    m_FrameConstants.mSunColor = m_RenderWorld.GetSun().mColor;
    m_FrameConstants.mSunColor.a *= m_FrameConstants.mExposure;
    m_FrameConstants.mSunDirection = Vec4(m_RenderWorld.GetSun().mDirection, 0.0f);
    m_FrameConstants.mCameraPosition = Vec4(vp.GetPosition(), 1.0f);
    m_FrameConstants.mViewportSize = inViewport.GetRenderSize();
    m_FrameConstants.mNrOfLights = uint32_t(m_RenderWorld.GetLights().size());
    m_FrameConstants.mViewMatrix = vp.GetView();
    m_FrameConstants.mInvViewMatrix = glm::inverse(vp.GetView());
    m_FrameConstants.mProjectionMatrix = final_proj_matrix;
    m_FrameConstants.mInvProjectionMatrix = glm::inverse(final_proj_matrix);
    m_FrameConstants.mPrevViewProjectionMatrix = m_FrameConstants.mViewProjectionMatrix;
    m_FrameConstants.mViewProjectionMatrix = final_proj_matrix * vp.GetView();
    m_FrameConstants.mInvViewProjectionMatrix = glm::inverse(final_proj_matrix * vp.GetView());

    if (m_FrameCounter == 0)
    {
        m_FrameConstants.mPrevJitter = m_FrameConstants.mJitter;
        m_FrameConstants.mPrevViewProjectionMatrix = m_FrameConstants.mViewProjectionMatrix;
    }

    // GPU driven debug line buffers, accessable from any shader
    m_FrameConstants.mDebugLinesVertexBuffer = inDevice.GetBindlessHeapIndex(m_DebugLinesVertexBuffer);
    m_FrameConstants.mDebugLinesIndirectArgsBuffer = inDevice.GetBindlessHeapIndex(m_DebugLinesIndirectArgsBuffer);

    const uint64_t lighting_key = GetLightingKey();

    if (lighting_key != m_LightingKey)
        m_DDGIFastConvergeFrames = sDDGIFastConvergeFrameCount;
    else if (m_DDGIFastConvergeFrames > 0)
        m_DDGIFastConvergeFrames--;

    m_LightingKey = lighting_key;
    RenderSettings::mDDGIIrradianceHysteresis = m_DDGIFastConvergeFrames > 0 ? sDDGIFastConvergeHysteresis : sDDGIHysteresis;

    Slice<const RTGeometry> geometries = m_RenderWorld.GetGeometries();

    if (geometries.size() != m_PrevGeometries.size() || std::memcmp(geometries.data(), m_PrevGeometries.data(), geometries.size_bytes()) != 0)
    {
        m_PrevGeometries.assign(geometries.begin(), geometries.end());
        m_DDGIRelocationFrames = DDGI_RELOCATION_FRAMES;
    }
    else if (m_DDGIRelocationFrames > 0)
        m_DDGIRelocationFrames--;

    RenderSettings::mDDGIRelocateAllProbes = m_DDGIRelocationFrames > 0;

    if (m_Settings.mDoPathTrace)
    {
        const uint64_t path_trace_key = GetPathTraceKey(lighting_key);

        if (path_trace_key != m_PathTraceKey)
            RenderSettings::mPathTraceReset = true;

        m_PathTraceKey = path_trace_key;
    }



    // handle PIX capture requests
    if (m_ShouldCaptureNextFrame)
    {
        PIXCaptureParameters capture_params = {};
        capture_params.GpuCaptureParameters.FileName = L"temp_pix_capture.wpix";
        gThrowIfFailed(PIXBeginCapture(PIX_CAPTURE_GPU, &capture_params));
    }

    static const int& upload_tlas = g_CVariables->Create("upload_scene", 1, true);
    // Start recording pending scene changes to the copy cmd list
    CommandList& copy_cmd_list = GetBackBufferData().mCopyCmdList;
    CommandList& direct_cmd_list = GetBackBufferData().mDirectCmdList;
    CommandList& update_cmd_list = GetBackBufferData().mUpdateCmdList;

    copy_cmd_list.Reset();
    direct_cmd_list.Reset();
    update_cmd_list.Reset();

    UpdateFontAtlas(inDevice);

    {
        PROFILE_SCOPE_GPU(direct_cmd_list, "OnRender");

        {
            PIXScopedEvent(static_cast<ID3D12GraphicsCommandList*>( copy_cmd_list ), PIX_COLOR(0, 255, 0), "UPLOAD SCENE");

            inDevice.FlushUploads(copy_cmd_list);

            if (upload_tlas)
                m_GPUScene.Upload(inDevice, copy_cmd_list, m_RenderWorld);
        }

        m_FrameConstants.mTLAS = m_GPUScene.HasTLAS() ? m_GPUScene.GetTLASDescriptorIndex() : m_GPUScene.GetEmptyTLASDescriptorIndex();
        m_FrameConstants.mShadowTLAS = m_RenderWorld.GetSun().mEnabled ? m_FrameConstants.mTLAS : m_GPUScene.GetEmptyTLASDescriptorIndex();
        m_FrameConstants.mLightsBuffer = m_GPUScene.GetLightsDescriptorIndex();
        m_FrameConstants.mMaterialsBuffer = m_GPUScene.GetMaterialsDescriptorIndex();
        m_FrameConstants.mInstancesBuffer = m_GPUScene.GetInstancesDescriptorIndex();

        //// Submit all copy commands
        copy_cmd_list.Close();
        copy_cmd_list.Submit(inDevice, inDevice.GetGraphicsQueue());

        update_cmd_list.Close();
        update_cmd_list.Submit(inDevice, inDevice.GetGraphicsQueue());

        // Record the entire frame into the direct cmd list
        m_RenderGraph.Execute(inDevice, m_FrameConstants, direct_cmd_list);

        RecordReadbacks(inDevice, direct_cmd_list);

        // Record commands to render ImGui to the backbuffer
        if (inApp->GetConfigSettings().mShowUI)
        {
            PROFILE_SCOPE_GPU(direct_cmd_list, "ImGui");
            ResolveImGuiDisplayTexture(inDevice);
            RenderImGui(m_RenderGraph, inDevice, direct_cmd_list, GetBackBufferData().mBackBuffer);
        }

    }

    g_GPUProfiler->Resolve(inDevice, direct_cmd_list);

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

        gThrowIfFailed(m_Swapchain->Present(sync_interval, present_flags), *inDevice);

        m_PrevFrameIndex = m_FrameIndex;
        m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

        const uint64_t new_fence_value = GetBackBufferData().mFenceValue + 1;
        GetPrevBackBufferData().mFenceValue = new_fence_value;

        gThrowIfFailed(inDevice.GetGraphicsQueue()->Signal(m_Fence.Get(), new_fence_value));
    }
    // );

    m_FrameCounter++;

    if (m_ShouldCaptureNextFrame)
    {
        // Wait for the present job here to make sure we capture everything
        if (m_PresentJobPtr)
            m_PresentJobPtr->Wait();

        gThrowIfFailed(PIXEndCapture(FALSE));
        m_ShouldCaptureNextFrame = false;
        ShellExecute(0, 0, "temp_pix_capture.wpix", 0, 0, SW_SHOW);
    }
}



void Renderer::Recompile(Device& inDevice)
{
    m_RenderGraph.Clear(inDevice);

    const DefaultTexturesData& default_textures = AddDefaultTexturesPass(m_RenderGraph, 
                                                                         inDevice, 
                                                                         m_BlackTexture, 
                                                                         m_WhiteTexture);
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

    const SkinningData& skinning_data = AddSkinningPass(m_RenderGraph, inDevice, m_RenderWorld);

    if (inDevice.IsRayTracingSupported())
        AddBuildAccelerationStructuresPass(m_RenderGraph, inDevice, m_RenderWorld, m_GPUScene);

    const SkyCubeData& sky_cube_data = AddSkyCubePass(m_RenderGraph, inDevice, m_RenderWorld);

    const DownsampleData& sky_cube_downsample_data = AddDownsamplePass(m_RenderGraph, inDevice, sky_cube_data.mSkyCubeTexture, "Skycube Prefilter");

    const ConvolveCubeData& convolved_cube_data = AddConvolveSkyCubePass(m_RenderGraph, inDevice, m_RenderWorld, sky_cube_data);

    const IntegrateBrdfData& integrate_brdf_data = AddIntegrateBrdfPass(m_RenderGraph, inDevice);

    if (m_Settings.mDoPathTrace && inDevice.IsRayTracingSupported())
    {
        compose_input = AddPathTracePass(m_RenderGraph, inDevice, sky_cube_data, gbuffer_output).mOutputTexture;
    }
    else
    {
        gbuffer_output = AddGBufferPass(m_RenderGraph, inDevice, m_RenderWorld).mOutput;

        // const auto& grass_data = AddGrassRenderPass(m_RenderGraph, inDevice, gbuffer_data);

        if (m_Settings.mEnableShadows && inDevice.IsRayTracingSupported())
            rt_shadows_texture = AddRayTracedShadowsPass(m_RenderGraph, inDevice, gbuffer_output);

        if (m_Settings.mEnableGTAO)
            ao_texture = AddDenoisePasses(m_RenderGraph, inDevice, gbuffer_output, AddGTAOPass(m_RenderGraph, inDevice, gbuffer_output).mOutputTexture, "GTAO");

        if (m_Settings.mEnableRTAO && inDevice.IsRayTracingSupported())
            ao_texture = AddAmbientOcclusionPass(m_RenderGraph, inDevice, gbuffer_output);

        const bool enable_ddgi = m_Settings.mEnableDDGI && inDevice.IsRayTracingSupported();

        if (enable_ddgi)
            ddgi_output = AddDDGIPass(m_RenderGraph, inDevice, m_GPUScene, gbuffer_output, sky_cube_data);

        if (m_Settings.mEnableReflections && inDevice.IsRayTracingSupported())
            reflections_texture = AddReflectionsPass(m_RenderGraph, inDevice, gbuffer_output, sky_cube_data, convolved_cube_data, enable_ddgi ? &ddgi_output : nullptr).mOutputTexture;

        const TiledLightCullingData& light_cull_data = AddTiledLightCullingPass(m_RenderGraph, inDevice);

        const LightingData& light_data = AddLightingPass(m_RenderGraph, inDevice, 
                                                         gbuffer_output, light_cull_data, integrate_brdf_data.outputTexture, sky_cube_data.mSkyCubeTexture, convolved_cube_data.mConvolvedCubeTexture, 
                                                         rt_shadows_texture, reflections_texture, ao_texture, ddgi_output.mOutput,
                                                         reflections_texture != default_textures.mBlackTexture, ddgi_output.mOutput != default_textures.mBlackTexture);

        compose_input = light_data.mOutputTexture;

        if (m_Settings.mEnableDDGI && m_Settings.mDebugProbes && inDevice.IsRayTracingSupported())
        {
            if (m_ProbeDebugMesh.indices.empty())
                CreateProbeDebugMesh(inDevice);

            AddProbeDebugPass(m_RenderGraph, inDevice, m_ProbeDebugMesh, ddgi_output, light_data.mOutputTexture, gbuffer_output.mDepthTexture);
        }

        if (m_Settings.mEnableDDGI && m_Settings.mDebugProbeRays && inDevice.IsRayTracingSupported())
            AddProbeDebugRaysPass(m_RenderGraph, inDevice, light_data.mOutputTexture, gbuffer_output.mDepthTexture, m_DebugLinesVertexBuffer, m_DebugLinesIndirectArgsBuffer);

        AddTransparentForwardPass(m_RenderGraph, inDevice, m_RenderWorld, gbuffer_output, light_data.mOutputTexture,
                                  integrate_brdf_data.outputTexture, sky_cube_data.mSkyCubeTexture, convolved_cube_data.mConvolvedCubeTexture,
                                  enable_ddgi ? &ddgi_output : nullptr, m_Settings.mEnableShadows && inDevice.IsRayTracingSupported());

        if (m_Settings.mEnableSSR)
            AddSSRTracePass(m_RenderGraph, inDevice, gbuffer_output, compose_input).mOutputTexture;

        if (m_Settings.mEnableTAA)
            compose_input = AddTAAResolvePass(m_RenderGraph, inDevice, gbuffer_output, light_data.mOutputTexture).mOutputTexture;

    }

    RenderGraphResourceID bloom_output = compose_input;

    const EDebugTexture debug_texture = EDebugTexture(m_Settings.mDebugTexture);

    // turn off any post processing effects for debug textures (this might change in the future)
    if (debug_texture == DEBUG_TEXTURE_NONE)
    {
        if (m_Settings.mEnableDoF)
            compose_input = AddDepthOfFieldPass(m_RenderGraph, inDevice, compose_input, gbuffer_output.mDepthTexture).mOutputTexture;

        if (m_Settings.mEnableBloom)
            bloom_output = AddBloomPass(m_RenderGraph, inDevice, compose_input).mOutputTexture;

        if (m_Settings.mEnableDebugOverlay)
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
                compose_input = ddgi_output.mOutput;
                break;
        }

        bloom_output = compose_input;
    }

    // pick an upscaler if TAA is disabled
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

    // apply post processing, tonemapping etc.
    const ComposeData& compose_data = AddComposePass(m_RenderGraph, inDevice, bloom_output, compose_input);

    RenderGraphResourceID final_output = compose_data.mOutputTexture;

    const bool has_gbuffer_depth = !m_Settings.mDoPathTrace;
    const bool wireframe_depth_test = m_Settings.mWireframe == WIREFRAME_MODE_OVERLAY && has_gbuffer_depth;

    if (m_Settings.mWireframe != WIREFRAME_MODE_OFF && debug_texture == DEBUG_TEXTURE_NONE)
    {
        const bool matches_render_size = m_RenderGraph.GetViewport().GetRenderSize() == m_RenderGraph.GetViewport().GetDisplaySize();
        AddWireframePass(m_RenderGraph, inDevice, m_RenderWorld, final_output, gbuffer_output.mDepthTexture, wireframe_depth_test && matches_render_size);
    }

    // Render UI on top
    const SDFUIData& sdfui_data = AddSDFUIPass(m_RenderGraph, inDevice, final_output, m_FontAtlasTexture);

    switch (debug_texture)
    {
        case DEBUG_TEXTURE_SSR:
            final_output = ssr_texture;
            break;
        case DEBUG_TEXTURE_GTAO:
            final_output = ao_texture;
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

    if (m_Settings.mWireframe != WIREFRAME_MODE_OFF && debug_texture != DEBUG_TEXTURE_NONE)
        AddWireframePass(m_RenderGraph, inDevice, m_RenderWorld, final_output, gbuffer_output.mDepthTexture, wireframe_depth_test);

    m_EntityTexture = gbuffer_output.mSelectionTexture;
    m_DisplayResource = final_output;
    m_DisplayTexture = AddPreImGuiPass(m_RenderGraph, inDevice, final_output).mDisplayTextureSRV;

    // const auto& imgui_data = AddImGuiPass(m_RenderGraph, inDevice, inStagingHeap, compose_data.mOutputTexture);

    m_RenderGraph.Compile(inDevice, m_GlobalConstants);

    m_RenderGraphKey = GetRenderGraphKey();
}



void Renderer::WaitForIdle(Device& inDevice)
{
    if (m_PresentJobPtr)
        m_PresentJobPtr->Wait();

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



uint64_t Renderer::GetViewportKey(const Viewport& inViewport) const
{
    const std::array key_data =
    {
        uint64_t(inViewport.GetDisplaySize().x),
        uint64_t(inViewport.GetDisplaySize().y),
        uint64_t(m_Settings.mEnableTAA),
        uint64_t(m_Upscaler.GetActiveUpscaler()),
        uint64_t(m_Upscaler.GetActiveUpscalerQuality())
    };

    return gHashFNV1a((const char*)key_data.data(), sizeof(key_data[0]) * key_data.size());
}



uint64_t Renderer::GetRenderGraphKey() const
{
    const RenderSun& sun = m_RenderWorld.GetSun();

    const std::array key_data =
    {
        uint64_t(m_Settings.mEnableDDGI),
        uint64_t(m_Settings.mDebugProbeRays),
        uint64_t(m_Settings.mDebugProbes),
        uint64_t(m_Settings.mEnableDebugOverlay),
        uint64_t(m_Settings.mEnableRTAO),
        uint64_t(m_Settings.mEnableGTAO),
        uint64_t(m_Settings.mEnableSSR),
        uint64_t(m_Settings.mEnableShadows),
        uint64_t(m_Settings.mEnableReflections),
        uint64_t(m_Settings.mEnableTAA),
        uint64_t(m_Settings.mEnableDoF),
        uint64_t(m_Settings.mEnableBloom),
        uint64_t(m_Settings.mDoPathTrace),
        uint64_t(m_Upscaler.GetActiveUpscaler()),
        uint64_t(m_Settings.mDebugTexture),
        uint64_t(m_Settings.mWireframe),
        uint64_t(RenderSettings::mDDGIProbeCount.x),
        uint64_t(RenderSettings::mDDGIProbeCount.y),
        uint64_t(RenderSettings::mDDGIProbeCount.z),
        uint64_t(RenderSettings::mDDGICascadeCount),
        uint64_t(sun.mEnabled),
        uint64_t(sun.mCubeMap.GetValue())
    };

    return gHashFNV1a((const char*)key_data.data(), sizeof(key_data[0]) * key_data.size());
}



uint64_t Renderer::GetLightingKey() const
{
    Slice<const RTMaterial> materials = m_RenderWorld.GetMaterials();
    Slice<const RTLight> lights = m_RenderWorld.GetLights();

    uint64_t hash = gHashFNV1a((const char*)&m_FrameConstants.mSunColor, sizeof(m_FrameConstants.mSunColor));
    hash = gHashFNV1a((const char*)&m_FrameConstants.mSunDirection, sizeof(m_FrameConstants.mSunDirection), hash);
    hash = gHashFNV1a((const char*)&m_FrameConstants.mSunConeAngle, sizeof(m_FrameConstants.mSunConeAngle), hash);
    hash = gHashFNV1a((const char*)materials.data(), materials.size_bytes(), hash);
    hash = gHashFNV1a((const char*)lights.data(), lights.size_bytes(), hash);

    return hash;
}



uint64_t Renderer::GetPathTraceKey(uint64_t inLightingKey) const
{
    Slice<const RTGeometry> geometries = m_RenderWorld.GetGeometries();

    uint64_t hash = gHashFNV1a((const char*)&m_FrameConstants.mViewProjectionMatrix, sizeof(m_FrameConstants.mViewProjectionMatrix), inLightingKey);
    hash = gHashFNV1a((const char*)geometries.data(), geometries.size_bytes(), hash);

    return hash;
}



TextureID Renderer::GetEntityTexture() const
{
    return m_RenderGraph.GetResources().GetTexture(m_EntityTexture);
}



TextureID Renderer::GetDisplayTexture() const
{
    return m_RenderGraph.GetResources().GetTextureView(m_DisplayTexture);
}



void Renderer::ResolveImGuiDisplayTexture(Device& inDevice)
{
    ImDrawData* draw_data = ImGui::GetDrawData();

    if (draw_data == nullptr)
        return;

    const DescriptorHeap& descriptor_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const ImTextureID proxy_texture_id = (ImTextureID)descriptor_heap.GetGPUDescriptorHandle(m_DisplayTextureProxy).ptr;
    const ImTextureID display_texture_id = (ImTextureID)inDevice.GetGPUDescriptorHandle(GetDisplayTexture()).ptr;

    for (ImDrawList* cmd_list : draw_data->CmdLists)
    {
        for (ImDrawCmd& cmd : cmd_list->CmdBuffer)
        {
            if (cmd.TextureId == proxy_texture_id)
                cmd.TextureId = display_texture_id;
        }
    }
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



uint64_t Renderer::RequestEntityPick(UVec2 inPixel)
{
    m_PendingEntityPickPixel = inPixel;
    m_PendingEntityPickID = ++m_EntityPickRequestCounter;

    return m_PendingEntityPickID;
}



bool Renderer::GetEntityPickResult(uint64_t inRequestID, Entity& outEntity) const
{
    if (inRequestID == 0 || m_EntityPickResultID != inRequestID)
        return false;

    outEntity = m_EntityPickResult;
    return true;
}



void Renderer::ResolveReadbacks(Device& inDevice)
{
    Readback& pick_readback = m_EntityPickReadbacks[m_FrameIndex];

    if (pick_readback.mBuffer.IsValid())
    {
        const uint32_t* mapped_ptr = nullptr;
        const CD3DX12_RANGE read_range = CD3DX12_RANGE(0, sizeof(uint32_t));

        gThrowIfFailed(inDevice.GetBuffer(pick_readback.mBuffer)->Map(0, &read_range, (void**)&mapped_ptr));
        m_EntityPickResult = Entity(*mapped_ptr);
        m_EntityPickResultID = pick_readback.mRequestID;
        inDevice.GetBuffer(pick_readback.mBuffer)->Unmap(0, nullptr);

        inDevice.ReleaseBuffer(pick_readback.mBuffer);
        pick_readback = {};
    }

    Readback& screenshot_readback = m_ScreenshotReadbacks[m_FrameIndex];

    if (screenshot_readback.mBuffer.IsValid())
    {
        const uint8_t* mapped_ptr = nullptr;
        const CD3DX12_RANGE read_range = CD3DX12_RANGE(0, inDevice.GetBuffer(screenshot_readback.mBuffer).GetSize());

        gThrowIfFailed(inDevice.GetBuffer(screenshot_readback.mBuffer)->Map(0, &read_range, (void**)&mapped_ptr));

        const bool is_bgra = screenshot_readback.mFormat == DXGI_FORMAT_B8G8R8A8_UNORM || screenshot_readback.mFormat == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

        SharedPtr<Array<uint8_t>> pixels = std::make_shared<Array<uint8_t>>(screenshot_readback.mSize.x * screenshot_readback.mSize.y * 4);

        for (uint32_t row = 0; row < screenshot_readback.mSize.y; row++)
        {
            const uint8_t* src = mapped_ptr + row * screenshot_readback.mRowPitch;
            uint8_t* dst = pixels->data() + row * screenshot_readback.mSize.x * 4;

            for (uint32_t pixel = 0; pixel < screenshot_readback.mSize.x; pixel++)
            {
                dst[pixel * 4 + 0] = src[pixel * 4 + ( is_bgra ? 2 : 0 )];
                dst[pixel * 4 + 1] = src[pixel * 4 + 1];
                dst[pixel * 4 + 2] = src[pixel * 4 + ( is_bgra ? 0 : 2 )];
                dst[pixel * 4 + 3] = 255;
            }
        }

        inDevice.GetBuffer(screenshot_readback.mBuffer)->Unmap(0, nullptr);
        inDevice.ReleaseBuffer(screenshot_readback.mBuffer);

        g_JobSystem.Schedule([pixels, file = screenshot_readback.mFile, size = screenshot_readback.mSize]()
        {
            if (stbi_write_png(file.string().c_str(), size.x, size.y, 4, pixels->data(), size.x * 4))
                gLogInfo("Renderer", "Saved screenshot to {}", file.string());
            else
                gLogError("Renderer", "Failed to write screenshot to {}", file.string());
        }, JOB_PRIORITY_LOW);

        screenshot_readback = {};
    }
}



void Renderer::RecordReadbacks(Device& inDevice, CommandList& inCmdList)
{
    auto CopyTextureToReadback = [&](TextureID inTexture, const D3D12_BOX* inBox, Readback& ioReadback, const char* inDebugName)
    {
        Texture& texture = inDevice.GetTexture(inTexture);
        ID3D12Resource* resource = texture.GetD3D12Resource();

        const UVec2 size = inBox ? UVec2(inBox->right - inBox->left, inBox->bottom - inBox->top) : UVec2(texture.GetWidth(), texture.GetHeight());

        D3D12_RESOURCE_DESC desc = resource->GetDesc();
        desc.Width = size.x;
        desc.Height = size.y;
        desc.MipLevels = 1;
        desc.DepthOrArraySize = 1;

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
        uint64_t total_size = 0;
        inDevice->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &total_size);

        ioReadback.mSize = size;
        ioReadback.mFormat = desc.Format;
        ioReadback.mRowPitch = footprint.Footprint.RowPitch;
        ioReadback.mBuffer = inDevice.CreateBuffer(Buffer::Describe(total_size, Buffer::READBACK, true, inDebugName));

        const D3D12_RESOURCE_STATES state = GetD3D12ResourceStates(texture.GetUsage());

        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, state, D3D12_RESOURCE_STATE_COPY_SOURCE);
        inCmdList->ResourceBarrier(1, &barrier);

        const CD3DX12_TEXTURE_COPY_LOCATION src = CD3DX12_TEXTURE_COPY_LOCATION(resource, 0);
        const CD3DX12_TEXTURE_COPY_LOCATION dst = CD3DX12_TEXTURE_COPY_LOCATION(inDevice.GetD3D12Resource(ioReadback.mBuffer), footprint);
        inCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, inBox);

        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        inCmdList->ResourceBarrier(1, &barrier);
    };

    if (m_PendingEntityPickID != 0)
    {
        const UVec2 pixel = m_PendingEntityPickPixel;
        const uint64_t request_id = m_PendingEntityPickID;
        m_PendingEntityPickID = 0;

        const TextureID entity_texture = GetEntityTexture();
        const Texture& texture = inDevice.GetTexture(entity_texture);

        if (pixel.x < texture.GetWidth() && pixel.y < texture.GetHeight())
        {
            const CD3DX12_BOX box = CD3DX12_BOX(pixel.x, pixel.y, pixel.x + 1, pixel.y + 1);
            CopyTextureToReadback(entity_texture, &box, m_EntityPickReadbacks[m_FrameIndex], "EntityPickReadback");
            m_EntityPickReadbacks[m_FrameIndex].mRequestID = request_id;
        }
        else
        {
            m_EntityPickResult = Entity::Null;
            m_EntityPickResultID = request_id;
        }
    }

    if (!m_PendingScreenshot.empty())
    {
        const TextureID display_texture = m_RenderGraph.GetResources().GetTexture(m_DisplayResource);
        const DXGI_FORMAT format = inDevice.GetTexture(display_texture).GetDesc().format;

        const bool is_supported = format == DXGI_FORMAT_R8G8B8A8_UNORM || format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
                                  format == DXGI_FORMAT_B8G8R8A8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

        if (!is_supported)
            gLogError("Renderer", "Can't save a screenshot of the current output, its format isn't 8 bits per channel. Turn off the debug texture and try again.");
        else if (!m_ScreenshotReadbacks[m_FrameIndex].mBuffer.IsValid())
        {
            m_ScreenshotReadbacks[m_FrameIndex].mFile = m_PendingScreenshot;
            CopyTextureToReadback(display_texture, nullptr, m_ScreenshotReadbacks[m_FrameIndex], "ScreenshotReadback");
        }

        m_PendingScreenshot.clear();
    }
}



TextureID InitImGui(Device& inDevice, DXGI_FORMAT inRtvFormat, uint32_t inFrameCount)
{
    int width = 0, height = 0;
    unsigned char* pixels = nullptr;
    ImGui::GetIO().Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

    TextureID font_texture = inDevice.CreateTexture(
    {
        .format = DXGI_FORMAT_R8_UNORM,
        .width  = uint32_t(width),
        .height = uint32_t(height),
        .usage  = Texture::Usage::SHADER_READ_ONLY,
        .debugName = "FontTexture"
    });

    DescriptorID font_texture_view = inDevice.GetTexture(font_texture).GetDescriptor();
    DescriptorHeap& descriptor_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.CommandQueue = inDevice.GetGraphicsQueue();
    init_info.Device = *inDevice;
    init_info.RTVFormat = inRtvFormat;
    init_info.NumFramesInFlight = sFrameCount;
    init_info.LegacySingleSrvCpuDescriptor = descriptor_heap.GetCPUDescriptorHandle(font_texture_view);
    init_info.LegacySingleSrvGpuDescriptor = descriptor_heap.GetGPUDescriptorHandle(font_texture_view);

    ImGui_ImplDX12_Init(&init_info);
    ImGui_ImplDX12_CreateDeviceObjects();

    //auto imgui_id = (void*)(intptr_t)inDevice.GetBindlessHeapIndex(font_texture_id);
    //ImGui::GetIO().Fonts->SetTexID(imgui_id);

    return font_texture;
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

    const D3D12_VIEWPORT bb_viewport = CD3DX12_VIEWPORT(inDevice.GetTexture(inBackBuffer).GetD3D12Resource());
    const D3D12_RECT bb_scissor = CD3DX12_RECT(bb_viewport.TopLeftX, bb_viewport.TopLeftY, bb_viewport.Width, bb_viewport.Height);

    inCmdList->RSSetViewports(1, &bb_viewport);
    inCmdList->RSSetScissorRects(1, &bb_scissor);

    const DescriptorHeap& rtv_heap = inDevice.GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    const std::array rtv = { rtv_heap.GetCPUDescriptorHandle(inDevice.GetTexture(inBackBuffer).GetDescriptor()) };

    inCmdList->OMSetRenderTargets(rtv.size(), rtv.data(), FALSE, nullptr);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), inCmdList);

    {
        auto backbuffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetD3D12Resource(inBackBuffer), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON);
        inCmdList->ResourceBarrier(1, &backbuffer_barrier);
    }
}


} // raekor
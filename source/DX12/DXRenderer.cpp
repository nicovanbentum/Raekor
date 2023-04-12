#include "pch.h"
#include "DXRenderer.h"

#include "shared.h"
#include "DXRenderGraph.h"

#include "Raekor/OS.h"
#include "Raekor/gui.h"
#include "Raekor/timer.h"
#include "Raekor/application.h"

namespace Raekor::DX12 {

Renderer::Renderer(Device& inDevice, const Viewport& inViewport, SDL_Window* inWindow) : 
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

    m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

    for (const auto& [index, backbuffer_data] : gEnumerate(m_BackBufferData)) {
        auto rtv_resource = ResourceRef{};
        gThrowIfFailed(m_Swapchain->GetBuffer(index, IID_PPV_ARGS(rtv_resource.GetAddressOf())));
        backbuffer_data.mBackBuffer = inDevice.CreateTextureView(rtv_resource, Texture::Desc{ .usage = Texture::Usage::RENDER_TARGET });

        backbuffer_data.mCmdList = CommandList(inDevice);
        backbuffer_data.mCmdList->SetName(L"Raekor::DX12::CommandList");

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
    m_Swapchain->GetDesc(&desc);
    gThrowIfFailed(m_Swapchain->ResizeBuffers(desc.BufferCount, inViewport.size.x, inViewport.size.y, desc.BufferDesc.Format, desc.Flags));

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

    std::cout << "Render Size: " << inViewport.size.x << " , " << inViewport.size.y << '\n';
}


void Renderer::OnRender(Device& inDevice, const Viewport& inViewport, Scene& inScene, DescriptorID inTLAS, DescriptorID inInstancesBuffer, DescriptorID inMaterialsBuffer, float inDeltaTime) {
    bool need_recompile = false;

    for (auto& [filename, shader_entry] : inDevice.m_Shaders) {
        auto error_code = std::error_code();
        auto timestamp = FileSystem::last_write_time(shader_entry.mPath, error_code);

        while (error_code)
            timestamp = FileSystem::last_write_time(shader_entry.mPath, error_code);

        if (timestamp > shader_entry.mLastWriteTime) {
            if (auto new_blob = sCompileShaderDXC(shader_entry.mPath))
                shader_entry.mBlob = new_blob;

            shader_entry.mLastWriteTime = FileSystem::last_write_time(shader_entry.mPath);
            need_recompile = true;
        }
    }

    GUI::BeginFrame();
    ImGui_ImplDX12_NewFrame();

    static bool open = true;
    ImGui::SetNextWindowBgAlpha(0.95f);
    ImGui::Begin("Settings", &open, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Frame %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    if (ImGui::Button("PIX GPU Capture")) 
        m_ShouldCaptureNextFrame = true;

    ImGui::Checkbox("Enable V-Sync", (bool*)&m_Settings.mEnableVsync);

    enum EGizmo { SUNLIGHT_DIR, DDGI_POS };
    constexpr auto items = std::array{ "Sunlight Direction", "DDGI Position", };
    static auto gizmo = EGizmo::SUNLIGHT_DIR;

    auto index = int(gizmo);
    if (ImGui::Combo("##EGizmo", &index, items.data(), int(items.size())))
        gizmo = EGizmo(index);

    need_recompile |= ImGui::Checkbox("Enable FSR 2.1", (bool*)&m_Settings.mEnableFsr2);
    need_recompile |= ImGui::Checkbox("Enable DDGI", (bool*)&m_Settings.mEnableDDGI);
    need_recompile |= ImGui::Checkbox("Show GI Probe Grid", (bool*)&m_Settings.mProbeDebug);
    need_recompile |= ImGui::Checkbox("Show GI Probe Rays", (bool*)&m_Settings.mDebugLines);

    if (need_recompile) {
        WaitForIdle(inDevice);
        Recompile(inDevice, inScene, inTLAS, inInstancesBuffer, inMaterialsBuffer);
    }

    auto lightView = inScene.view<DirectionalLight, Transform>();

    if (lightView.begin() != lightView.end()) {
        auto& sun_transform = lightView.get<Transform>(lightView.front());

        auto sun_rotation_degrees = glm::degrees(glm::eulerAngles(sun_transform.rotation));

        if (ImGui::DragFloat3("Sun Angle", glm::value_ptr(sun_rotation_degrees), 0.1f, -360.0f, 360.0f, "%.1f")) {
            sun_transform.rotation = glm::quat(glm::radians(sun_rotation_degrees));
            sun_transform.Compose();
        }
    }

    ImGui::NewLine(); ImGui::Separator();

    if (auto grass_pass = m_RenderGraph.GetPass<GrassData>()) {
        ImGui::Text("Grass Settings");
        ImGui::DragFloat("Grass Bend", &grass_pass->GetData().mRenderConstants.mBend, 0.01f, -1.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Grass Tilt", &grass_pass->GetData().mRenderConstants.mTilt, 0.01f, -1.0f, 1.0f, "%.2f");
        ImGui::DragFloat2("Wind Direction", glm::value_ptr(grass_pass->GetData().mRenderConstants.mWindDirection), 0.01f, -10.0f, 10.0f, "%.1f");
        ImGui::NewLine(); ImGui::Separator();
    }


    if (auto rtao_pass = m_RenderGraph.GetPass<RTAOData>()) {
        auto& params = rtao_pass->GetData().mParams;
        ImGui::Text("RTAO Settings");
        ImGui::DragFloat("Radius", &params.mRadius, 0.01f, 0.0f, 20.0f, "%.2f");
        ImGui::DragFloat("Intensity", &params.mIntensity, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Normal Bias", &params.mNormalBias, 0.001f, 0.0f, 1.0f, "%.3f");
        ImGui::SliderInt("Sample Count", (int*)&params.mSampleCount, 1u, 128u);
        ImGui::NewLine(); ImGui::Separator();
    }

    if (auto ddgi_pass = m_RenderGraph.GetPass<ProbeTraceData>()) {
        auto& data = ddgi_pass->GetData();
        ImGui::Text("DDGI Settings");
        ImGui::DragInt3("Debug Probe", glm::value_ptr(data.mDebugProbe));
        ImGui::DragFloat3("Probe Spacing", glm::value_ptr(data.mDDGIData.mProbeSpacing), 0.01f, -1000.0f, 1000.0f, "%.3f");
        ImGui::DragInt3("Probe Count", glm::value_ptr(data.mDDGIData.mProbeCount), 1, 1, 40);
        ImGui::DragFloat3("Grid Position", glm::value_ptr(data.mDDGIData.mCornerPosition), 0.01f, -1000.0f, 1000.0f, "%.3f");
    }

    auto debug_textures = std::vector<TextureID>{};

    if (auto ddgi_pass = m_RenderGraph.GetPass<ProbeUpdateData>()) {
        debug_textures.push_back(ddgi_pass->GetData().mRaysDepthTexture.mResourceTexture);
        debug_textures.push_back(ddgi_pass->GetData().mRaysIrradianceTexture.mResourceTexture);
    }

    if (auto ddgi_pass = m_RenderGraph.GetPass<ProbeDebugData>()) {
        debug_textures.push_back(ddgi_pass->GetData().mProbesDepthTexture.mResourceTexture);
        debug_textures.push_back(ddgi_pass->GetData().mProbesIrradianceTexture.mResourceTexture);
    }

    auto texture_strs = std::vector<std::string>(debug_textures.size());
    auto texture_cstrs = std::vector<const char*>(debug_textures.size());

    static auto texture_index = glm::min(3, int(debug_textures.size() -1 ));
    texture_index = glm::min(texture_index, int(debug_textures.size() -1 ));

    for (auto [index, texture] : gEnumerate(debug_textures)) {
        texture_strs[index] = gGetDebugName(inDevice.GetTexture(texture).GetResource().Get());
        texture_cstrs[index] = texture_strs[index].c_str();
    }

    ImGui::Combo("Debug View", &texture_index, texture_cstrs.data(), texture_cstrs.size());

    auto texture = inDevice.GetTexture(debug_textures[texture_index]);
    auto gpu_handle = inDevice.GetGPUDescriptorHandle(debug_textures[texture_index]);

    ImGui::Image((void*)((intptr_t)gpu_handle.ptr), ImVec2(texture.GetDesc().width, texture.GetDesc().height));

    ImGui::NewLine(); ImGui::Separator();

    if (ImGui::Button("Save As GraphViz..")) {
        const auto file_path = OS::sSaveFileDialog("DOT File (*.dot)\0", "dot");

        if (!file_path.empty()) {
            auto ofs = std::ofstream(file_path);
            ofs << m_RenderGraph.ToGraphVizText(inDevice);
        }
    }

    ImGui::End();

    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
    ImGuizmo::SetRect(0, 0, float(inViewport.size.x), float(inViewport.size.y));

    switch (gizmo) {
        case EGizmo::SUNLIGHT_DIR: {
            if (lightView.begin() != lightView.end()) {
                auto& sun_transform = lightView.get<Transform>(lightView.front());

                const auto manipulated = ImGuizmo::Manipulate(
                    glm::value_ptr(inViewport.GetCamera().GetView()),
                    glm::value_ptr(inViewport.GetCamera().GetProjection()),
                    ImGuizmo::OPERATION::ROTATE, ImGuizmo::MODE::WORLD,
                    glm::value_ptr(sun_transform.localTransform)
                );

                if (manipulated)
                    sun_transform.Decompose();
        } break;
        case EGizmo::DDGI_POS: {
            if (auto ddgi = m_RenderGraph.GetPass<ProbeTraceData>()) {
                auto transform = glm::translate(glm::mat4(1.0f), ddgi->GetData().mDDGIData.mCornerPosition);

                const auto manipulated = ImGuizmo::Manipulate(
                    glm::value_ptr(inViewport.GetCamera().GetView()),
                    glm::value_ptr(inViewport.GetCamera().GetProjection()),
                    ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::MODE::WORLD,
                    glm::value_ptr(transform)
                );

                if (manipulated) {
                    Vec3 scale, translation, skew;
                    glm::quat rotation;
                    Vec4 perspective;
                    glm::decompose(transform, scale, rotation, translation, skew, perspective);

                    ddgi->GetData().mDDGIData.mCornerPosition = translation;
                }
            }
        } break;
    }

    }

    GUI::EndFrame();

    auto& backbuffer_data = GetBackBufferData();
    auto  completed_value = m_Fence->GetCompletedValue();

    if (completed_value < backbuffer_data.mFenceValue) {
        gThrowIfFailed(m_Fence->SetEventOnCompletion(backbuffer_data.mFenceValue, m_FenceEvent));
        WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
    }

    m_ElapsedTime += inDeltaTime;
    const auto& camera = m_RenderGraph.GetViewport().GetCamera();

    const auto jitter_phase_count = ffxFsr2GetJitterPhaseCount(m_RenderGraph.GetViewport().size.x, m_RenderGraph.GetViewport().size.y);

    float jitter_offset_x = 0;
    float jitter_offset_y = 0;
    ffxFsr2GetJitterOffset(&jitter_offset_x, &jitter_offset_y, m_FrameCounter, jitter_phase_count);

    // Calculate the jittered projection matrix.
    const auto jitter_x = 2.0f * jitter_offset_x / (float)m_RenderGraph.GetViewport().size.x;
    const auto jitter_y = -2.0f * jitter_offset_y / (float)m_RenderGraph.GetViewport().size.y;
    const auto offset_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(jitter_x, jitter_y, 0));
    const auto jittered_proj_matrix = camera.GetProjection();
    //const auto jittered_proj_matrix = offset_matrix * camera.GetProjection();

    m_FrameConstants.mTime                       = m_ElapsedTime;
    m_FrameConstants.mDeltaTime                  = inDeltaTime;
    m_FrameConstants.mFrameIndex                 = 0; // TODO: not used anywhere yet, mainly for padding atm
    m_FrameConstants.mFrameCounter               = m_FrameCounter;
    m_FrameConstants.mSunDirection               = glm::vec4(inScene.GetSunLightDirection(), 0.0f);
    m_FrameConstants.mCameraPosition             = glm::vec4(camera.GetPosition(), 1.0f);
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

    backbuffer_data.mCmdList.Reset();

    m_RenderGraph.SetBackBuffer(backbuffer_data.mBackBuffer);

    m_RenderGraph.Execute(inDevice, backbuffer_data.mCmdList, m_FrameCounter);

    RenderImGui(m_RenderGraph, inDevice, backbuffer_data.mCmdList);

    backbuffer_data.mCmdList.Close();

    const auto commandLists = std::array { static_cast<ID3D12CommandList*>(backbuffer_data.mCmdList) };
    inDevice.GetQueue()->ExecuteCommandLists(commandLists.size(), commandLists.data());

    auto sync_interval = m_Settings.mEnableVsync;
    auto present_flags = 0u;

    if (!m_Settings.mEnableVsync && inDevice.IsTearingSupported())
        present_flags = DXGI_PRESENT_ALLOW_TEARING;

    gThrowIfFailed(m_Swapchain->Present(sync_interval, present_flags));

    backbuffer_data.mFenceValue++;
    m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();
    gThrowIfFailed(inDevice.GetQueue()->Signal(m_Fence.Get(), GetBackBufferData().mFenceValue));

    m_FrameCounter++;

    if (m_ShouldCaptureNextFrame) {
        gThrowIfFailed(PIXEndCapture(FALSE));
        m_ShouldCaptureNextFrame = false;
        ShellExecute(0, 0, "temp_pix_capture.wpix", 0, 0, SW_SHOW);
    }
}


void Renderer::Recompile(Device& inDevice, const Scene& inScene, DescriptorID inTLAS, DescriptorID inInstancesBuffer, DescriptorID inMaterialsBuffer) {
    m_RenderGraph.Clear(inDevice);
    m_RenderGraph.SetBackBuffer(GetBackBufferData().mBackBuffer);

    const auto& gbuffer_data = AddGBufferPass(m_RenderGraph, inDevice, inScene);

    // const auto& grass_data   = AddGrassRenderPass(m_RenderGraph, inDevice, gbuffer_data);
    
    const auto& shadow_data  = AddShadowMaskPass(m_RenderGraph, inDevice, gbuffer_data, inTLAS);

    const auto& rtao_data = AddAmbientOcclusionPass(m_RenderGraph, inDevice, gbuffer_data, inTLAS);

    const auto& reflection_data = AddReflectionsPass(m_RenderGraph, inDevice, gbuffer_data, inTLAS, inInstancesBuffer, inMaterialsBuffer);

    const auto& downsample_data = AddDownsamplePass(m_RenderGraph, inDevice, reflection_data.mOutputTexture);
    
    const auto& ddgi_trace_data = AddProbeTracePass(m_RenderGraph, inDevice, inTLAS, inInstancesBuffer, inMaterialsBuffer);

    const auto& ddgi_update_data = AddProbeUpdatePass(m_RenderGraph, inDevice, ddgi_trace_data);
    
    const auto& light_data = AddLightingPass(m_RenderGraph, inDevice, gbuffer_data, shadow_data, reflection_data, rtao_data, ddgi_update_data);
    
    if (m_Settings.mProbeDebug)
        const auto& probe_debug_data = AddProbeDebugPass(m_RenderGraph, inDevice, ddgi_trace_data, ddgi_update_data, light_data.mOutputTexture, gbuffer_data.mDepthTexture);

    if (m_Settings.mDebugLines) {
        const auto& debug_lines_data = AddDebugLinesPass(m_RenderGraph, inDevice, light_data.mOutputTexture, gbuffer_data.mDepthTexture);
        m_FrameConstants.mDebugLinesVertexBuffer = debug_lines_data.mVertexBuffer.GetBindlessIndex(inDevice);
        m_FrameConstants.mDebugLinesIndirectArgsBuffer = debug_lines_data.mIndirectArgsBuffer.GetBindlessIndex(inDevice);
    }

    auto compose_input = light_data.mOutputTexture;
    
    if (m_Settings.mEnableFsr2)
        compose_input = AddFsrPass(m_RenderGraph, inDevice, m_Fsr2Context, light_data.mOutputTexture, gbuffer_data).mOutputTexture;

    const auto& compose_data = AddComposePass(m_RenderGraph, inDevice, compose_input);

    m_RenderGraph.Compile(inDevice);
}


void Renderer::WaitForIdle(Device& inDevice) {
    for (auto& backbuffer_data : m_BackBufferData) {
        gThrowIfFailed(inDevice.GetQueue()->Signal(m_Fence.Get(), backbuffer_data.mFenceValue));

        if (m_Fence->GetCompletedValue() < backbuffer_data.mFenceValue) {
            gThrowIfFailed(m_Fence->SetEventOnCompletion(backbuffer_data.mFenceValue, m_FenceEvent));
            WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
        }

        backbuffer_data.mFenceValue = 0;
    }

    m_Fence->Signal(0);
    m_FrameCounter = 0;
}


CommandList& Renderer::StartSingleSubmit() {
    auto& cmd_list = GetBackBufferData().mCmdList;
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


RTTI_CLASS_CPP(GBufferData)	        {}
RTTI_CLASS_CPP(GrassData)           {}
RTTI_CLASS_CPP(RTShadowMaskData)    {}
RTTI_CLASS_CPP(RTAOData)            {}
RTTI_CLASS_CPP(ReflectionsData)     {}
RTTI_CLASS_CPP(DownsampleData)      {}
RTTI_CLASS_CPP(LightingData)        {}
RTTI_CLASS_CPP(FSR2Data)            {}
RTTI_CLASS_CPP(ProbeTraceData)      {}
RTTI_CLASS_CPP(ProbeUpdateData)     {}
RTTI_CLASS_CPP(ProbeDebugData)      {}
RTTI_CLASS_CPP(DebugLinesData)      {}
RTTI_CLASS_CPP(ComposeData)         {}

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
            .format = DXGI_FORMAT_D32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::DEPTH_STENCIL_TARGET,
        }, L"GBUFFER_DEPTH");

        inRenderPass->Create(render_texture);
        inRenderPass->Create(movec_texture);
        inRenderPass->Create(depth_texture);

        inData.mDepthTexture        = inRenderPass->Write(depth_texture);
        inData.mRenderTexture       = inRenderPass->Write(render_texture);       // SV_TARGET0
        inData.mMotionVectorTexture = inRenderPass->Write(movec_texture); // SV_TARGET1

        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, "GBufferVS", "GBufferPS");
        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(inData.mPipeline.GetAddressOf()));
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

        for (const auto& [entity, transform, mesh] : inScene.view<Transform, Mesh>().each()) {
            const auto& indexBuffer = inDevice.GetBuffer(BufferID(mesh.indexBuffer));
            const auto& vertexBuffer = inDevice.GetBuffer(BufferID(mesh.vertexBuffer));

            const auto index_view = D3D12_INDEX_BUFFER_VIEW {
                .BufferLocation = indexBuffer->GetGPUVirtualAddress(),
                .SizeInBytes    = uint32_t(mesh.indices.size() * sizeof(mesh.indices[0])),
                .Format         = DXGI_FORMAT_R32_UINT,
            };

            const auto vertex_view = D3D12_VERTEX_BUFFER_VIEW {
                .BufferLocation = vertexBuffer->GetGPUVirtualAddress(),
                .SizeInBytes    = uint32_t(vertexBuffer->GetDesc().Width),
                .StrideInBytes  = 44u // TODO: derive from input layout since its all tightly packed
            };

            if (mesh.material == sInvalidEntity)
                continue;

            auto material = inScene.try_get<Material>(mesh.material);
            if (material == nullptr)
                material = &Material::Default;

            auto root_constants = GbufferRootConstants {};
            root_constants.mAlbedo       = material->albedo;
            root_constants.mProperties.x = material->metallic;
            root_constants.mProperties.y = material->roughness;
            root_constants.mTextures.x   = material->gpuAlbedoMap;
            root_constants.mTextures.y   = material->gpuNormalMap;
            root_constants.mTextures.z   = material->gpuMetallicRoughnessMap;

            root_constants.mWorldTransform = transform.worldTransform;

            inCmdList->SetGraphicsRoot32BitConstants(0, sizeof(root_constants) / sizeof(DWORD), &root_constants, 0);

            inCmdList->IASetIndexBuffer(&index_view);
            inCmdList->IASetVertexBuffers(0, 1, &vertex_view);

            inCmdList->DrawIndexedInstanced(mesh.indices.size(), 1, 0, 0, 0);
        }
    });
}



const RTShadowMaskData& AddShadowMaskPass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferData& inGBufferData, DescriptorID inTLAS) {
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
        inData.mTopLevelAccelerationStructure   = inTLAS;

        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, "RTShadowsCS");
        gThrowIfFailed(inDevice->CreateComputePipelineState(&state, IID_PPV_ARGS(&inData.mPipeline)));
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



const RTAOData& AddAmbientOcclusionPass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferData& inGbufferData, DescriptorID inTLAS) {
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
        inData.mTopLevelAccelerationStructure   = inTLAS;

        inData.mParams.mRadius      = 2.0;
        inData.mParams.mIntensity   = 1.0;
        inData.mParams.mNormalBias  = 0.01;
        inData.mParams.mSampleCount = 4u;

        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, "RTAmbientOcclusionCS");
        gThrowIfFailed(inDevice->CreateComputePipelineState(&state, IID_PPV_ARGS(&inData.mPipeline)));
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

        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, "GrassRenderVS", "GrassRenderPS");
        state.InputLayout = {};
        state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&inData.mPipeline));

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

        inCmdList->IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->DrawInstanced(blade_vertex_count, 65536, 0, 0);
    });
}



const ReflectionsData& AddReflectionsPass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferData& inGBufferData, DescriptorID inTLAS, DescriptorID inInstancesBuffer, DescriptorID inMaterialsBuffer) {
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
        inData.mTopLevelAccelerationStructure = inTLAS;
        inData.mInstancesBuffer = inInstancesBuffer;
        inData.mMaterialBuffer = inMaterialsBuffer;

        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, "RTReflectionsCS");
        
        Timer timer;
        gThrowIfFailed(inDevice->CreateComputePipelineState(&pso_state, IID_PPV_ARGS(inData.mPipeline.GetAddressOf())));
        std::cout << std::format("CreateComputePipelineState took {} ms.\n", Timer::sToMilliseconds(timer.GetElapsedTime()));
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

        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, "DownsampleCS");
        inDevice->CreateComputePipelineState(&pso_state, IID_PPV_ARGS(inData.mPipeline.GetAddressOf()));
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
            .mGlobalAtomicBuffer = inDevice.GetBindlessHeapIndex(atomic_buffer.GetView()),
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



const ProbeTraceData& AddProbeTracePass(RenderGraph& inRenderGraph, Device& inDevice, DescriptorID inTLAS, DescriptorID inInstancesBuffer, DescriptorID inMaterialsBuffer) {
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

        inData.mInstancesBuffer = inInstancesBuffer;
        inData.mMaterialBuffer = inMaterialsBuffer;
        inData.mTopLevelAccelerationStructure = inTLAS;

        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, "ProbeTraceCS");
        gThrowIfFailed(inDevice->CreateComputePipelineState(&pso_state, IID_PPV_ARGS(&inData.mPipeline)));
    },
    [&inRenderGraph, &inDevice](ProbeTraceData& inData, CommandList& inCmdList)
    {
        auto Index3Dto1D = [](UVec3 inIndex, UVec3 inCount) {
            return inIndex.x + inIndex.y * inCount.x + inIndex.z * inCount.x * inCount.y;
        };
        
        inData.mDDGIData.mRaysDepthTexture        = inData.mRaysDepthTexture.GetBindlessIndex(inDevice);
        inData.mDDGIData.mRaysIrradianceTexture   = inData.mRaysIrradianceTexture.GetBindlessIndex(inDevice);

        auto constants = ProbeTraceRootConstants {
            .mInstancesBuffer       = inDevice.GetBindlessHeapIndex(inData.mInstancesBuffer),
            .mMaterialsBuffer       = inDevice.GetBindlessHeapIndex(inData.mMaterialBuffer),
            .mTLAS                  = inDevice.GetBindlessHeapIndex(inData.mTopLevelAccelerationStructure),
            .mDebugProbeIndex       = Index3Dto1D(inData.mDebugProbe, inData.mDDGIData.mProbeCount),
            .mDDGIData              = inData.mDDGIData,
            .mRandomRotationMatrix  = gRandomRotationMatrix()
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
            .height = uint32_t(DDGI_DEPTH_TEXELS * (total_probe_count / DDGI_PROBES_PER_ROW + 1)),
            .usage  = Texture::Usage::SHADER_READ_WRITE
        }, L"DDGI_UPDATE_DEPTH");

        const auto probes_irradiance_texture = inDevice.CreateTexture(Texture::Desc{
            .format = DXGI_FORMAT_R11G11B10_FLOAT,
            .width  = uint32_t(DDGI_IRRADIANCE_TEXELS * DDGI_PROBES_PER_ROW),
            .height = uint32_t(DDGI_IRRADIANCE_TEXELS * (total_probe_count / DDGI_PROBES_PER_ROW)),
            .usage  = Texture::Usage::SHADER_READ_WRITE
        }, L"DDGI_UPDATE_IRRADIANCE");

        inData.mProbesDepthTexture      = inRenderPass->CreateAndWrite(probes_depth_texture);
        inData.mProbesIrradianceTexture = inRenderPass->CreateAndWrite(probes_irradiance_texture);
        inData.mRaysDepthTexture        = inRenderPass->Read(inTraceData.mRaysDepthTexture);
        inData.mRaysIrradianceTexture   = inRenderPass->Read(inTraceData.mRaysIrradianceTexture);

        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, "ProbeUpdateDepthCS");
        gThrowIfFailed(inDevice->CreateComputePipelineState(&pso_state, IID_PPV_ARGS(&inData.mDepthPipeline)));

        pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, "ProbeUpdateIrradianceCS");
        gThrowIfFailed(inDevice->CreateComputePipelineState(&pso_state, IID_PPV_ARGS(&inData.mIrradiancePipeline)));
        
    },
    [&inDevice, &inTraceData](ProbeUpdateData& inData, CommandList& inCmdList)
    {
        inData.mDDGIData                          = inTraceData.mDDGIData;
        inData.mDDGIData.mProbesDepthTexture      = inData.mProbesDepthTexture.GetBindlessIndex(inDevice);
        inData.mDDGIData.mProbesIrradianceTexture = inData.mProbesIrradianceTexture.GetBindlessIndex(inDevice);
        inData.mDDGIData.mRaysDepthTexture        = inData.mRaysDepthTexture.GetBindlessIndex(inDevice);
        inData.mDDGIData.mRaysIrradianceTexture   = inData.mRaysIrradianceTexture.GetBindlessIndex(inDevice);

        inCmdList.PushComputeConstants(inData.mDDGIData);

        inCmdList->SetPipelineState(inData.mDepthPipeline.Get());
        const auto depth_texture = inDevice.GetTexture(inData.mProbesDepthTexture.mCreatedTexture);
        inCmdList->Dispatch(depth_texture.GetDesc().width / DDGI_DEPTH_TEXELS, depth_texture.GetDesc().height / DDGI_DEPTH_TEXELS, 1);
        
        inCmdList->SetPipelineState(inData.mIrradiancePipeline.Get());
        const auto irradiance_texture = inDevice.GetTexture(inData.mProbesIrradianceTexture.mCreatedTexture);
        inCmdList->Dispatch(irradiance_texture.GetDesc().width / DDGI_IRRADIANCE_TEXELS, irradiance_texture.GetDesc().height / DDGI_IRRADIANCE_TEXELS, 1);
    });
}



const ProbeDebugData& AddProbeDebugPass(RenderGraph& inRenderGraph, Device& inDevice, const ProbeTraceData& inTraceData, const ProbeUpdateData& inUpdateData, TextureResource inRenderTarget, TextureResource inDepthTarget) {
    return inRenderGraph.AddGraphicsPass<ProbeDebugData>("GI PROBE DEBUG PASS", inDevice,
    [&](IRenderPass* inRenderPass, ProbeDebugData& inData)
    {
        gGenerateSphere(inData.mProbeMesh, .5f, 8u, 8u);

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

        auto pso_desc = inDevice.CreatePipelineStateDesc(inRenderPass, "ProbeDebugVS", "ProbeDebugPS");
        gThrowIfFailed(inDevice->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(inData.mPipeline.GetAddressOf())));
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

        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, "DebugLinesVS", "DebugLinesPS");
        pso_state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso_state.RasterizerState.AntialiasedLineEnable = true;
        pso_state.InputLayout.NumElements = uint32_t(vertex_layout.size());
        pso_state.InputLayout.pInputElementDescs = vertex_layout.data();

        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(inData.mPipeline.GetAddressOf()));
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

        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, "FullscreenTriangleVS", "LightingPS");
        state.InputLayout = {}; // clear the input layout, we generate the fullscreen triangle inside the vertex shader
        state.DepthStencilState.DepthEnable = FALSE;
        state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&inData.mPipeline));
    },
    [&inRenderGraph, &inDevice, &inProbeData](LightingData& inData, CommandList& inCmdList)
    {
        auto root_constants = LightingRootConstants {
            .mShadowMaskTexture = inData.mShadowMaskTexture.GetBindlessIndex(inDevice),
            .mReflectionsTexture = inData.mReflectionsTexture.GetBindlessIndex(inDevice),
            .mGbufferDepthTexture = inData.mGBufferDepthTexture.GetBindlessIndex(inDevice),
            .mGbufferRenderTexture = inData.mGBufferRenderTexture.GetBindlessIndex(inDevice),
            .mAmbientOcclusionTexture = inData.mAmbientOcclusionTexture.GetBindlessIndex(inDevice),
            .mDDGIData = inProbeData.mDDGIData
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
        fsr2_dispatch_desc.cameraFovAngleVertical   = glm::radians(viewport.GetFov());

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
        inData.mInputTexture  = inRenderPass->Read(inInputTexture);
        const auto backbuffer = inRenderPass->Write(inRenderGraph.GetBackBuffer()); 
        assert(backbuffer.mResourceTexture == inRenderGraph.GetBackBuffer()); // backbuffer created with RTV, so Write should just return that

        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, "FullscreenTriangleVS", "FinalComposePS");
        state.InputLayout = {}; // clear the input layout, we generate the fullscreen triangle inside the vertex shader
        state.DepthStencilState.DepthEnable = FALSE;
        state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&inData.mPipeline));
    },

    [&inRenderGraph, &inDevice](ComposeData& inData, CommandList& inCmdList)
    {
        struct {
            uint32_t mInputTexture;
        } root_constants;

        root_constants.mInputTexture = inData.mInputTexture.GetBindlessIndex(inDevice);
        const auto backbuffer_id = inRenderGraph.GetBackBuffer();

        {   // manual barriers around the imported backbuffer resource, the rendergraph doesn't handle this kind of state
            auto backbuffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetResourcePtr(backbuffer_id), D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
            inCmdList->ResourceBarrier(1, &backbuffer_barrier);
        }

        inCmdList->SetPipelineState(inData.mPipeline.Get());

        const auto& backbuffer_texture = inDevice.GetTexture(backbuffer_id);
        const auto bb_width  = backbuffer_texture.GetResource()->GetDesc().Width;
        const auto bb_height = backbuffer_texture.GetResource()->GetDesc().Height;

        const auto scissor  = CD3DX12_RECT(0, 0, bb_width, bb_height);
        const auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(bb_width), float(bb_height));
        inCmdList->RSSetViewports(1, &viewport);
        inCmdList->RSSetScissorRects(1, &scissor);
        
        inCmdList->SetGraphicsRoot32BitConstants(0, sizeof(root_constants) / sizeof(DWORD), &root_constants, 0);
        inCmdList->DrawInstanced(6, 1, 0, 0);

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

    return font_texture_id;
}



void RenderImGui(RenderGraph& inRenderGraph, Device& inDevice, CommandList& inCmdList) {
    PIXScopedEvent(static_cast<ID3D12GraphicsCommandList*>(inCmdList), PIX_COLOR(0, 255, 0), "IMGUI PASS");

    // Just in-case we did some external pass like FSR2 before this that sets its own descriptor heaps
    inDevice.BindDrawDefaults(inCmdList);

    {   // manual barriers around the imported backbuffer resource, the rendergraph doesn't handle this kind of state
        auto backbuffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetResourcePtr(inRenderGraph.GetBackBuffer()), D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
        inCmdList->ResourceBarrier(1, &backbuffer_barrier);
    }

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
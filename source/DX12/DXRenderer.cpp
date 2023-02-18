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
    const auto swapchainDesc = DXGI_SWAP_CHAIN_DESC1 {
        .Width = inViewport.size.x,
        .Height = inViewport.size.y,
        .Format      = sSwapchainFormat,
        .SampleDesc  = DXGI_SAMPLE_DESC {.Count = 1 },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = sFrameCount,
        .SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD,
    };

    auto factory = ComPtr<IDXGIFactory4>{};
    gThrowIfFailed(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory)));

    auto wmInfo = SDL_SysWMinfo{};
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(inWindow, &wmInfo);
    auto hwnd = wmInfo.info.win.window;

    auto swapchain = ComPtr<IDXGISwapChain1>{};
    gThrowIfFailed(factory->CreateSwapChainForHwnd(inDevice.GetQueue(), hwnd, &swapchainDesc, nullptr, nullptr, &swapchain));
    gThrowIfFailed(swapchain.As(&m_Swapchain));

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


void Renderer::OnResize(Device& inDevice, const Viewport& inViewport) {
    WaitForIdle(inDevice);

    for (auto& bb_data : m_BackBufferData)
        inDevice.ReleaseTextureImmediate(bb_data.mBackBuffer);

    gThrowIfFailed(m_Swapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0));

    for (const auto& [index, backbuffer_data] : gEnumerate(m_BackBufferData)) {
        auto rtv_resource = ResourceRef(nullptr);
        gThrowIfFailed(m_Swapchain->GetBuffer(index, IID_PPV_ARGS(rtv_resource.GetAddressOf())));

        backbuffer_data.mBackBuffer = inDevice.CreateTextureView(rtv_resource, Texture::Desc{ .usage = Texture::Usage::RENDER_TARGET });
        rtv_resource->SetName(L"BACKBUFFER");
    }

    m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

    ffxFsr2ContextDestroy(&m_Fsr2Context);

    auto fsr2_desc = FfxFsr2ContextDescription{
        .flags          = FfxFsr2InitializationFlagBits::FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE,
        .maxRenderSize  = { inViewport.size.x, inViewport.size.y },
        .displaySize    = { inViewport.size.x, inViewport.size.y },
        .device         = ffxGetDeviceDX12(inDevice),
    };

    auto ffx_error = ffxFsr2ContextCreate(&m_Fsr2Context, &fsr2_desc);
    assert(ffx_error == FFX_OK);
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

    if (need_recompile) {
        WaitForIdle(inDevice);
        Recompile(inDevice, inScene, inTLAS, inInstancesBuffer, inMaterialsBuffer);
    }

    GUI::BeginFrame();
    ImGui_ImplDX12_NewFrame();
    
    static bool open = true;
    ImGui::Begin("Settings", &open, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Frame %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::Checkbox("Enable V-Sync", (bool*)&m_Settings.mEnableVsync);

    if (ImGui::Checkbox("Enable FSR 2.1", (bool*)&m_Settings.mEnableFsr2)) {
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

    if (auto grass_pass = m_RenderGraph.GetPass<GrassData>()) {
        ImGui::DragFloat("Grass Bend", &grass_pass->GetData().mRenderConstants.mBend, 0.01f, -1.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Grass Tilt", &grass_pass->GetData().mRenderConstants.mTilt, 0.01f, -1.0f, 1.0f, "%.2f");
        ImGui::DragFloat2("Wind Direction", glm::value_ptr(grass_pass->GetData().mRenderConstants.mWindDirection), 0.01f, -10.0f, 10.0f, "%.1f");
    }

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
    // const auto jittered_proj_matrix = offset_matrix * camera.GetProjection();


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

    backbuffer_data.mCmdList.Reset();

    m_RenderGraph.SetBackBuffer(backbuffer_data.mBackBuffer);

    m_RenderGraph.Execute(inDevice, backbuffer_data.mCmdList, m_FrameCounter);

    RenderImGui(m_RenderGraph, inDevice, backbuffer_data.mCmdList);

    backbuffer_data.mCmdList.Close();

    const auto commandLists = std::array{ static_cast<ID3D12CommandList*>(backbuffer_data.mCmdList) };
    inDevice.GetQueue()->ExecuteCommandLists(commandLists.size(), commandLists.data());

    gThrowIfFailed(m_Swapchain->Present(m_Settings.mEnableVsync, 0));

    backbuffer_data.mFenceValue++;
    m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();
    gThrowIfFailed(inDevice.GetQueue()->Signal(m_Fence.Get(), GetBackBufferData().mFenceValue));

    m_FrameCounter++;
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
    
    const auto& light_data = AddLightingPass(m_RenderGraph, inDevice, gbuffer_data, shadow_data, reflection_data, rtao_data);
    
    auto compose_input = light_data.mOutputTexture;

    if (m_Settings.mEnableFsr2) {
        compose_input = AddFsrPass(m_RenderGraph, inDevice, m_Fsr2Context, light_data.mOutputTexture, gbuffer_data).mOutputTexture;
    }

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

    const auto cmd_lists = std::array{ (ID3D12CommandList*)inCmdList };
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
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
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

        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, "RTAmbientOcclusionCS");
        gThrowIfFailed(inDevice->CreateComputePipelineState(&state, IID_PPV_ARGS(&inData.mPipeline)));
    },
    [&inRenderGraph, &inDevice](RTAOData& inData, CommandList& inCmdList) 
    {
        auto& viewport = inRenderGraph.GetViewport();

        const auto root_constants = ShadowMaskRootConstants {
            .mGbufferRenderTexture  = inData.mGBufferRenderTexture.GetBindlessIndex(inDevice),
            .mGbufferDepthTexture   = inData.mGbufferDepthTexture.GetBindlessIndex(inDevice),
            .mShadowMaskTexture     = inData.mOutputTexture.GetBindlessIndex(inDevice),
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
        gThrowIfFailed(inDevice->CreateComputePipelineState(&pso_state, IID_PPV_ARGS(&inData.mPipeline)));
    },
    [&inRenderGraph, &inDevice](ReflectionsData& inData, CommandList& inCmdList) 
    {
        auto& viewport = inRenderGraph.GetViewport();

        const auto root_constants = ReflectionsRootConstants {
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



const ProbeTraceData& AddProbeTracePass(RenderGraph& inRenderGraph, Device& inDevice, DescriptorID inTLAS) {
    return inRenderGraph.AddComputePass<ProbeTraceData>("GI TRACE PROBES PASS", inDevice,
    [&](IRenderPass* inRenderPass, ProbeTraceData& inData) 
    {
        const auto depth_texture = inDevice.CreateTexture(Texture::Desc{
            .format = DXGI_FORMAT_R16G16_FLOAT,
            .width  = 16,
            .height = 16,
            .usage  = Texture::Usage::SHADER_READ_WRITE
        }, L"GI_PROBE_DEPTH");

        const auto irradiance_texture = inDevice.CreateTexture(Texture::Desc{
            .format = DXGI_FORMAT_R11G11B10_FLOAT,
            .width  = 6,
            .height = 6,
            .usage  = Texture::Usage::SHADER_READ_WRITE
        }, L"GI_PROBE_IRRADIANCE");

        inRenderPass->Create(depth_texture);
        inRenderPass->Create(irradiance_texture);
        inData.mDepthTexture = inRenderPass->Write(depth_texture);
        inData.mIrradianceTexture = inRenderPass->Write(irradiance_texture);

        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, "ProbeTraceCS");
        gThrowIfFailed(inDevice->CreateComputePipelineState(&pso_state, IID_PPV_ARGS(&inData.mPipeline)));

    },
    [&inDevice, inTLAS](ProbeTraceData& inData, CommandList& inCmdList) 
    {   
        const auto constants = ProbeTraceRootConstants{
            .mTLAS          = inDevice.GetBindlessHeapIndex(inTLAS),
            .mDispatchSize  = { 1, DDGI_RAYS_PER_WAVE, 1 },
            .mProbeWorldPos = inData.mProbeWorldPos
        };

        inCmdList.PushComputeConstants(constants);
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->Dispatch(constants.mDispatchSize.x, constants.mDispatchSize.y, constants.mDispatchSize.z);
    });
}



const LightingData& AddLightingPass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferData& inGBufferData, const RTShadowMaskData& inShadowMaskData, const ReflectionsData& inReflectionsData, const RTAOData& inAmbientOcclusionData) {
    return inRenderGraph.AddGraphicsPass<LightingData>("LIGHT PASS", inDevice,
    [&](IRenderPass* inRenderPass, LightingData& inData)
    {
        const auto output_texture = inDevice.CreateTexture({
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET
        }, L"LIGHT_OUTPUT");

        inRenderPass->Create(output_texture);

        inData.mOutputTexture           = inRenderPass->Write(output_texture);
        inData.mShadowMaskTexture       = inRenderPass->Read(inShadowMaskData.mOutputTexture);
        inData.mReflectionsTexture      = inRenderPass->Read(inReflectionsData.mOutputTexture);
        inData.mGBufferDepthTexture     = inRenderPass->Read(inGBufferData.mDepthTexture);
        inData.mGBufferRenderTexture    = inRenderPass->Read(inGBufferData.mRenderTexture);
        inData.mAmbientOcclusionTexture = inRenderPass->Read(inAmbientOcclusionData.mOutputTexture);

        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, "FullscreenTriangleVS", "LightingPS");
        state.InputLayout = {}; // clear the input layout, we generate the fullscreen triangle inside the vertex shader
        state.DepthStencilState.DepthEnable = FALSE;
        state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&inData.mPipeline));
    },
    [&inRenderGraph, &inDevice](LightingData& inData, CommandList& inCmdList)
    {
        const auto root_constants = LightingRootConstants {
            .mShadowMaskTexture       = inData.mShadowMaskTexture.GetBindlessIndex(inDevice),
            .mReflectionsTexture      = inData.mReflectionsTexture.GetBindlessIndex(inDevice),
            .mGbufferDepthTexture     = inData.mGBufferDepthTexture.GetBindlessIndex(inDevice),
            .mGbufferRenderTexture    = inData.mGBufferRenderTexture.GetBindlessIndex(inDevice),
            .mAmbientOcclusionTexture = inData.mAmbientOcclusionTexture.GetBindlessIndex(inDevice),
        };

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
        auto& vp = inRenderGraph.GetViewport();

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
        fsr2_dispatch_desc.motionVectorScale.x      = float(vp.size.x);
        fsr2_dispatch_desc.motionVectorScale.y      = float(vp.size.y);
        fsr2_dispatch_desc.reset                    = false;
        fsr2_dispatch_desc.enableSharpening         = false;
        fsr2_dispatch_desc.sharpness                = 0.0f;
        fsr2_dispatch_desc.frameTimeDelta           = Timer::sToMilliseconds(inData.mDeltaTime);
        fsr2_dispatch_desc.preExposure              = 1.0f;
        fsr2_dispatch_desc.renderSize.width         = vp.size.x;
        fsr2_dispatch_desc.renderSize.height        = vp.size.y;
        fsr2_dispatch_desc.cameraFar                = vp.GetCamera().GetFar();
        fsr2_dispatch_desc.cameraNear               = vp.GetCamera().GetNear();
        fsr2_dispatch_desc.cameraFovAngleVertical   = glm::radians(vp.GetFov());

        const auto jitter_phase_count = ffxFsr2GetJitterPhaseCount(vp.size.x, vp.size.x);
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
        const auto bb_width = backbuffer_texture.GetResource()->GetDesc().Width;
        const auto bb_height = backbuffer_texture.GetResource()->GetDesc().Height;

        const auto scissor = CD3DX12_RECT(0, 0, bb_width, bb_height);
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
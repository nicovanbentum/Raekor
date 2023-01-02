#include "pch.h"
#include "DXRenderer.h"
#include "shared.h"
#include "DXRenderGraph.h"
#include "Raekor/timer.h"
#include "Raekor/gui.h"

namespace Raekor::DX {

Renderer::Renderer(Device& inDevice, const Viewport& inViewport, SDL_Window* inWindow) : 
    m_RenderGraph(inDevice, inViewport, sFrameCount) 
{
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
    swapchainDesc.BufferCount = sFrameCount;
    swapchainDesc.Width = inViewport.size.x;
    swapchainDesc.Height = inViewport.size.y;
    swapchainDesc.Format = sSwapchainFormat;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGIFactory4> factory;
    gThrowIfFailed(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory)));

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(inWindow, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;

    ComPtr<IDXGISwapChain1> swapchain;
    gThrowIfFailed(factory->CreateSwapChainForHwnd(inDevice.GetQueue(), hwnd, &swapchainDesc, nullptr, nullptr, &swapchain));
    gThrowIfFailed(swapchain.As(&m_Swapchain));

    m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

    for (const auto& [index, backbuffer_data] : gEnumerate(m_BackBufferData)) {
        ResourceRef rtv_resource;
        gThrowIfFailed(m_Swapchain->GetBuffer(index, IID_PPV_ARGS(rtv_resource.GetAddressOf())));
        backbuffer_data.mBackBuffer = inDevice.CreateTextureView(rtv_resource, Texture::Desc{ .usage = Texture::Usage::RENDER_TARGET });

        backbuffer_data.mCmdList = CommandList(inDevice);
        backbuffer_data.mCmdList->SetName(L"Raekor::DX::CommandList");

        rtv_resource->SetName(L"BACKBUFFER");
    }

    inDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));
    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (!m_FenceEvent)
        gThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));

    auto fsr2_desc = FfxFsr2ContextDescription{};
    fsr2_desc.flags = FfxFsr2InitializationFlagBits::FFX_FSR2_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS | FFX_FSR2_ENABLE_AUTO_EXPOSURE;
    fsr2_desc.displaySize = { inViewport.size.x, inViewport.size.y };
    fsr2_desc.maxRenderSize = { inViewport.size.x, inViewport.size.y };
    fsr2_desc.device = ffxGetDeviceDX12(inDevice);

    m_FsrScratchMemory.resize(ffxFsr2GetScratchMemorySizeDX12());
    auto ffx_error = ffxFsr2GetInterfaceDX12(&fsr2_desc.callbacks, inDevice, m_FsrScratchMemory.data(), m_FsrScratchMemory.size());
    assert(ffx_error == FFX_OK);

    ffx_error = ffxFsr2ContextCreate(&m_Fsr2Context, &fsr2_desc);
    assert(ffx_error == FFX_OK);
}


void Renderer::OnRender(Device& inDevice, float inDeltaTime) {
    auto& backbuffer_data = GetBackBufferData();
    auto  completed_value = m_Fence->GetCompletedValue();

    if (completed_value < backbuffer_data.mFenceValue) {
        gThrowIfFailed(m_Fence->SetEventOnCompletion(backbuffer_data.mFenceValue, m_FenceEvent));
        WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
    }

    // Update any per pass data
    if (auto fsr_pass = m_RenderGraph.GetPass<FSR2Data>())
        fsr_pass->GetData().mDeltaTime = inDeltaTime;

    // Reset the command list
    backbuffer_data.mCmdList.Reset();

    // Update the render graph's internal backbuffer reference
    m_RenderGraph.SetBackBuffer(backbuffer_data.mBackBuffer);

    m_RenderGraph.Execute(inDevice, backbuffer_data.mCmdList, m_FrameCounter);

    RenderImGui(m_RenderGraph, inDevice, backbuffer_data.mCmdList);

    backbuffer_data.mCmdList.Close();

    const auto commandLists = std::array{ static_cast<ID3D12CommandList*>(backbuffer_data.mCmdList) };
    inDevice.GetQueue()->ExecuteCommandLists(commandLists.size(), commandLists.data());

    gThrowIfFailed(m_Swapchain->Present(0, 0));

    backbuffer_data.mFenceValue++;
    m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();
    gThrowIfFailed(inDevice.GetQueue()->Signal(m_Fence.Get(), GetBackBufferData().mFenceValue));

    m_FrameCounter++;
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
}


void Renderer::Recompile(Device& inDevice, const Scene& inScene, DescriptorID inTLAS) {
    m_RenderGraph.Clear(inDevice);
    m_RenderGraph.SetBackBuffer(GetBackBufferData().mBackBuffer);

    const auto& gbuffer_data = AddGBufferPass(m_RenderGraph, inDevice, inScene);

    const auto& grass_data   = AddGrassRenderPass(m_RenderGraph, inDevice, gbuffer_data);
    
    const auto& shadow_data  = AddShadowMaskPass(m_RenderGraph, inDevice, inScene, gbuffer_data, inTLAS);
    
    const auto& light_data   = AddLightingPass(m_RenderGraph, inDevice, inScene, gbuffer_data, shadow_data);
    
    auto compose_input = light_data.mOutputTexture;

    if (m_Settings.mEnableFsr2) {
        compose_input = AddFsrPass(m_RenderGraph, inDevice, m_Fsr2Context, light_data.mOutputTexture, gbuffer_data).mOutputTexture;
    }

    const auto& compose_data = AddComposePass(m_RenderGraph, inDevice, compose_input);

    m_RenderGraph.Compile(inDevice);
}


void Renderer::WaitForIdle(Device& inDevice) {
    for (const auto& backbuffer_data : m_BackBufferData) {
        gThrowIfFailed(inDevice.GetQueue()->Signal(m_Fence.Get(), backbuffer_data.mFenceValue));

        if (m_Fence->GetCompletedValue() < backbuffer_data.mFenceValue) {
            gThrowIfFailed(m_Fence->SetEventOnCompletion(backbuffer_data.mFenceValue, m_FenceEvent));
            WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
        }
    }
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


RTTI_CLASS_CPP(GBufferData)	    {}
RTTI_CLASS_CPP(GrassData)       {}
RTTI_CLASS_CPP(ShadowMaskData)  {}
RTTI_CLASS_CPP(LightingData)    {}
RTTI_CLASS_CPP(FSR2Data)        {}
RTTI_CLASS_CPP(ComposeData)     {}


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

        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, "gbufferVS", "gbufferPS");
        inDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&inData.mPipeline));
        },

    [&](GBufferData& inData, CommandList& inCmdList)
    {
        const auto& camera = inRenderGraph.GetViewport().GetCamera();

        auto frame_constants = FrameConstants{};
        frame_constants.mSunDirection = glm::vec4(inScene.GetSunLightDirection(), 0.0f);
        frame_constants.mCameraPosition = glm::vec4(camera.GetPosition(), 1.0f);
        frame_constants.mViewMatrix = camera.GetView();
        frame_constants.mProjectionMatrix = camera.GetProjection();
        frame_constants.mViewProjectionMatrix = camera.GetProjection() * camera.GetView();
        frame_constants.mInvViewProjectionMatrix = glm::inverse(camera.GetProjection() * camera.GetView());

        inRenderGraph.GetPerFrameAllocator().AllocAndCopy(frame_constants, inRenderGraph.GetPerFrameAllocatorOffset());

        const auto& viewport = inRenderGraph.GetViewport();
        inCmdList.SetViewportScissorRect(viewport);
        inCmdList->SetPipelineState(inData.mPipeline.Get());

        const auto clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        const auto clear_rect = CD3DX12_RECT(0, 0, viewport.size.x, viewport.size.y);
        inCmdList->ClearRenderTargetView(inDevice.GetHeapPtr(inData.mRenderTexture), glm::value_ptr(clear_color), 1, &clear_rect);
        inCmdList->ClearRenderTargetView(inDevice.GetHeapPtr(inData.mMotionVectorTexture), glm::value_ptr(clear_color), 1, &clear_rect);
        inCmdList->ClearDepthStencilView(inDevice.GetHeapPtr(inData.mDepthTexture), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 1, &clear_rect);

        auto root_constants = GbufferRootConstants{};
        root_constants.mViewProj = viewport.GetCamera().GetProjection()  * viewport.GetCamera().GetView();
        inCmdList->SetGraphicsRoot32BitConstants(0, sizeof(GbufferRootConstants) / sizeof(DWORD), &root_constants, 0);

        for (const auto& [entity, mesh] : inScene.view<Mesh>().each()) {
            const auto& indexBuffer = inDevice.GetBuffer(BufferID(mesh.indexBuffer));
            const auto& vertexBuffer = inDevice.GetBuffer(BufferID(mesh.vertexBuffer));

            auto index_view = D3D12_INDEX_BUFFER_VIEW{};
            index_view.BufferLocation = indexBuffer->GetGPUVirtualAddress();
            index_view.Format = DXGI_FORMAT_R32_UINT;
            index_view.SizeInBytes = mesh.indices.size() * sizeof(mesh.indices[0]);

            auto vertex_view = D3D12_VERTEX_BUFFER_VIEW{};
            vertex_view.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
            vertex_view.SizeInBytes = vertexBuffer->GetDesc().Width;
            vertex_view.StrideInBytes = 44; // TODO: derive from input layout since its all tightly packed

            auto material = inScene.try_get<Material>(mesh.material);
            if (material == nullptr)
                material = &Material::Default;

            root_constants.mAlbedo = material->albedo;
            root_constants.mProperties.x = material->metallic;
            root_constants.mProperties.y = material->roughness;
            root_constants.mTextures.x = material->gpuAlbedoMap;
            root_constants.mTextures.y = material->gpuNormalMap;
            root_constants.mTextures.z = material->gpuMetallicRoughnessMap;
            inCmdList->SetGraphicsRoot32BitConstants(0, sizeof(root_constants) / sizeof(DWORD), &root_constants, 0);

            inCmdList->IASetIndexBuffer(&index_view);
            inCmdList->IASetVertexBuffers(0, 1, &vertex_view);

            inCmdList->DrawIndexedInstanced(mesh.indices.size(), 1, 0, 0, 0);
        }
    });
}



const ShadowMaskData& AddShadowMaskPass(RenderGraph& inRenderGraph, Device& inDevice, const Scene& inScene, const GBufferData& inGBufferData, DescriptorID inTLAS) {
    return inRenderGraph.AddComputePass<ShadowMaskData>("SHADOW PASS", inDevice,
    [&](IRenderPass* inRenderPass, ShadowMaskData& inData)
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

        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, "shadowsCS");
        gThrowIfFailed(inDevice->CreateComputePipelineState(&state, IID_PPV_ARGS(&inData.mPipeline)));
    },

    [&](ShadowMaskData& inData, CommandList& inCmdList)
    {
        ShadowMaskRootConstants root_constants = {};
        root_constants.mTextures.x = inData.mGBufferRenderTexture.GetBindlessIndex(inDevice);
        root_constants.mTextures.y = inData.mGbufferDepthTexture.GetBindlessIndex(inDevice);
        root_constants.mTextures.z = inData.mOutputTexture.GetBindlessIndex(inDevice);
        root_constants.mTextures.w = inDevice.GetBindlessHeapIndex(inData.mTopLevelAccelerationStructure);

        auto& vp = inRenderGraph.GetViewport();
        root_constants.invViewProj   = glm::inverse(vp.GetCamera().GetProjection() * vp.GetCamera().GetView());
        root_constants.mLightDir     = glm::vec4(inScene.GetSunLightDirection(), 0);
        
        root_constants.mFrameCounter++;
        root_constants.mDispatchSize = vp.size;

        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->SetComputeRoot32BitConstants(0, sizeof(ShadowMaskRootConstants) / sizeof(DWORD), &root_constants, 0);
        inCmdList->Dispatch(vp.size.x / 8, vp.size.y / 8, 1);
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
    },
    [&](GrassData& inData, CommandList& inCmdList)
    {
        const auto blade_vertex_count = 15;

        inCmdList->IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->DrawInstanced(blade_vertex_count, 1, 0, 0);
    });
}



const LightingData& AddLightingPass(RenderGraph& inRenderGraph, Device& inDevice, const Scene& inScene, const GBufferData& inGBufferData, const ShadowMaskData& inShadowMaskData) {
    return inRenderGraph.AddGraphicsPass<LightingData>("LIGHT PASS", inDevice,
    [&](IRenderPass* inRenderPass, LightingData& inData)
    {
        const auto output_texture = inDevice.CreateTexture({
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET
        }, L"LIGHT_OUTPUT");

        inRenderPass->Create(output_texture);

        inData.mOutputTexture         = inRenderPass->Write(output_texture);
        inData.mShadowMaskTexture     = inRenderPass->Read(inShadowMaskData.mOutputTexture);
        inData.mGBufferDepthTexture   = inRenderPass->Read(inGBufferData.mDepthTexture);
        inData.mGBufferRenderTexture  = inRenderPass->Read(inGBufferData.mRenderTexture);

        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, "blitVS", "pbrPS");
        state.InputLayout = {}; // clear the input layout, we generate the fullscreen triangle inside the vertex shader
        state.DepthStencilState.DepthEnable = FALSE;
        state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&inData.mPipeline));

        inRenderPass->ReserveMemory(sizeof(FrameConstants));
    },
    [&](LightingData& inData, CommandList& inCmdList)
    {
        auto root_constants = LightingRootConstants{};
        root_constants.mShadowMaskTexture      = inData.mShadowMaskTexture.GetBindlessIndex(inDevice);
        root_constants.mGbufferDepthTexture    = inData.mGBufferDepthTexture.GetBindlessIndex(inDevice);
        root_constants.mGbufferRenderTexture   = inData.mGBufferRenderTexture.GetBindlessIndex(inDevice);

        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.SetViewportScissorRect(inRenderGraph.GetViewport());
        inCmdList.PushConstants(root_constants);
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

    [&](FSR2Data& inData, CommandList& inCmdList)
    {
        auto& vp = inRenderGraph.GetViewport();

        const auto color_texture_ptr  = inDevice.GetResourcePtr(inData.mColorTexture.mCreatedTexture);
        const auto depth_texture_ptr  = inDevice.GetResourcePtr(inData.mDepthTexture.mCreatedTexture);
        const auto movec_texture_ptr  = inDevice.GetResourcePtr(inData.mMotionVectorTexture.mCreatedTexture);
        const auto output_texture_ptr = inDevice.GetResourcePtr(inData.mOutputTexture.mCreatedTexture);

        auto fsr2_dispatch_desc = FfxFsr2DispatchDescription();
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

        auto state = inDevice.CreatePipelineStateDesc(inRenderPass, "blitVS", "blitPS");
        state.InputLayout = {}; // clear the input layout, we generate the fullscreen triangle inside the vertex shader
        state.DepthStencilState.DepthEnable = FALSE;
        state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&inData.mPipeline));
    },

    [&](ComposeData& inData, CommandList& inCmdList) 
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
    int width, height;
    unsigned char* pixels;
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
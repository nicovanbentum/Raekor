#include "pch.h"
#include "DXRenderer.h"
#include "DXRenderGraph.h"

namespace Raekor::DX {

RTTI_CLASS_CPP(GBufferData)	    {}
RTTI_CLASS_CPP(ShadowMaskData)  {}
RTTI_CLASS_CPP(LightingData)    {}
RTTI_CLASS_CPP(ComposeData)     {}


const GBufferData& AddGBufferPass(RenderGraph& inRenderGraph, Device& inDevice, const Scene& inScene) {
    return inRenderGraph.AddGraphicsPass<GBufferData>("GBuffer Pass", inDevice,
        [&inRenderGraph, &inDevice, &inScene](IRenderPass* inRenderPass, GBufferData& inData)
        {
            inData.mRenderTexture = inDevice.CreateTexture(Texture::Desc{
                .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
                .width  = inRenderGraph.GetViewport().size.x,
                .height = inRenderGraph.GetViewport().size.y,
                .usage  = Texture::RENDER_TARGET,
                }, L"RT_GBUFFER_R32G32B32A32_FLOAT");

            inData.mDepthTexture = inDevice.CreateTexture(Texture::Desc{
                .format = DXGI_FORMAT_D32_FLOAT,
                .width  = inRenderGraph.GetViewport().size.x,
                .height = inRenderGraph.GetViewport().size.y,
                .usage  = Texture::DEPTH_STENCIL_TARGET,
                }, L"DT_GBUFFER_D32_FLOAT");

            inRenderPass->Create(inData.mDepthTexture);
            inRenderPass->Create(inData.mRenderTexture);

            inData.mDepthTexture = inRenderPass->Write(inData.mDepthTexture);
            inData.mRenderTexture = inRenderPass->Write(inData.mRenderTexture);

            auto state = inDevice.CreatePipelineStateDesc(inRenderPass, "gbufferVS", "gbufferPS");
            inDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&inData.mPipeline));
        },

        [&inRenderGraph, &inDevice, &inScene](const GBufferData& inData, CommandList& inCmdList)
        {
            struct RootConstants {
            glm::vec4	mAlbedo;
            glm::uvec4	mTextures;
            glm::vec4	mProperties;
            glm::mat4	mViewProj;
            } root_constants;

            const auto& viewport = inRenderGraph.GetViewport();
            inCmdList.SetViewportScissorRect(viewport);
            inCmdList->SetPipelineState(inData.mPipeline.Get());

            const auto clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
            const auto clear_rect = CD3DX12_RECT(0, 0, viewport.size.x, viewport.size.y);
            inCmdList->ClearRenderTargetView(inDevice.GetCPUDescriptorHandle(inData.mRenderTexture), glm::value_ptr(clear_color), 1, &clear_rect);
            inCmdList->ClearDepthStencilView(inDevice.GetCPUDescriptorHandle(inData.mDepthTexture), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 1, &clear_rect);

            root_constants.mViewProj = viewport.GetCamera().GetProjection() * viewport.GetCamera().GetView();
            inCmdList->SetGraphicsRoot32BitConstants(0, sizeof(RootConstants) / sizeof(DWORD), &root_constants, 0);

            for (const auto& [entity, mesh] : inScene.view<Mesh>().each()) {
                const auto& indexBuffer = inDevice.GetBuffer(BufferID(mesh.indexBuffer));
                const auto& vertexBuffer = inDevice.GetBuffer(BufferID(mesh.vertexBuffer));

                D3D12_INDEX_BUFFER_VIEW indexView = {};
                indexView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
                indexView.Format = DXGI_FORMAT_R32_UINT;
                indexView.SizeInBytes = mesh.indices.size() * sizeof(mesh.indices[0]);

                D3D12_VERTEX_BUFFER_VIEW vertexView = {};
                vertexView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
                vertexView.SizeInBytes = vertexBuffer->GetDesc().Width;
                vertexView.StrideInBytes = 44; // TODO: derive from input layout since its all tightly packed

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

                inCmdList->IASetIndexBuffer(&indexView);
                inCmdList->IASetVertexBuffers(0, 1, &vertexView);

                inCmdList->DrawIndexedInstanced(mesh.indices.size(), 1, 0, 0, 0);
            }
        });
}



const ShadowMaskData& AddShadowMaskPass(RenderGraph& inRenderGraph, Device& inDevice, const Scene& inScene, const GBufferData& inGBufferData, ResourceID inTLAS) {
    return inRenderGraph.AddComputePass<ShadowMaskData>("Ray-traced Shadow Mask Pass", inDevice,
        [&](IRenderPass* inRenderPass, ShadowMaskData& inData)
        {
            inData.mOutputTexture = inDevice.CreateTexture(Texture::Desc{
                .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
                .width  = inRenderGraph.GetViewport().size.x,
                .height = inRenderGraph.GetViewport().size.y,
                .usage  = Texture::Usage::SHADER_WRITE
            }, L"SHADOW_MASK");

            inData.mOutputTexture = inRenderPass->Write(inData.mOutputTexture);
            inData.mGbufferDepthTexture = inRenderPass->Read(inGBufferData.mDepthTexture);
            inData.mGBufferRenderTexture = inRenderPass->Read(inGBufferData.mRenderTexture);
            inData.mTopLevelAccelerationStructure = inTLAS;

            auto state = inDevice.CreatePipelineStateDesc(inRenderPass, "shadowsCS");
            gThrowIfFailed(inDevice->CreateComputePipelineState(&state, IID_PPV_ARGS(&inData.mPipeline)));
        },

        [&](const ShadowMaskData& inData, CommandList& inCmdList)
        {
            struct {
            glm::mat4	invViewProj     = {};
            glm::vec4	mLightDir       = {};
            glm::uvec4	mTextures       = {};
            glm::uvec2  mDispatchSize   = {};
            uint32_t	mFrameCounter   = 0;
            } root_constants;

            root_constants.mTextures.x = inDevice.GetBindlessHeapIndex(inData.mGBufferRenderTexture);
            root_constants.mTextures.y = inDevice.GetBindlessHeapIndex(inData.mGbufferDepthTexture);
            root_constants.mTextures.z = inDevice.GetBindlessHeapIndex(inData.mOutputTexture);
            root_constants.mTextures.w = inDevice.GetBindlessHeapIndex(inData.mTopLevelAccelerationStructure);

            auto& vp = inRenderGraph.GetViewport();
            root_constants.invViewProj = glm::inverse(vp.GetCamera().GetProjection() * vp.GetCamera().GetView());
            root_constants.mDispatchSize = vp.size;
            root_constants.mFrameCounter++;

            auto lightView = inScene.view<const DirectionalLight, const Transform>();
            auto lookDirection = glm::vec3(0.25f, -0.9f, 0.0f);

            // TODO: BUGGED?
            if (lightView.begin() != lightView.end()) {
                const auto& lightTransform = lightView.get<const Transform>(lightView.front());
                lookDirection = static_cast<glm::quat>(lightTransform.rotation) * lookDirection;
            }
            else {
                // we rotate default light a little or else we get nan values in our view matrix
                lookDirection = static_cast<glm::quat>(glm::vec3(glm::radians(15.0f), 0, 0)) * lookDirection;
            }

            lookDirection = glm::clamp(lookDirection, { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f });

            root_constants.mLightDir = glm::vec4(lookDirection, 0);

            inCmdList->SetPipelineState(inData.mPipeline.Get());
            inCmdList->SetComputeRoot32BitConstants(0, sizeof(root_constants) / sizeof(DWORD), &root_constants, 0);
            inCmdList->Dispatch(vp.size.x / 8, vp.size.y / 8, 1);
        });
}



const LightingData& AddLightingPass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferData& inGBufferData, const ShadowMaskData& inShadowMaskData) {
    return inRenderGraph.AddGraphicsPass<LightingData>("Compose Pass", inDevice,
        [&](IRenderPass* inRenderPass, LightingData& inData)
        {
            inData.mOutputTexture = inDevice.CreateTexture({
                .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
                .width  = inRenderGraph.GetViewport().size.x,
                .height = inRenderGraph.GetViewport().size.y,
                .usage  = Texture::RENDER_TARGET
            }, L"RT_DEFERRED_LIGHTING");

            inData.mGBufferDepthTexture = inRenderPass->Read(inGBufferData.mDepthTexture);
            inData.mGBufferRenderTexture = inRenderPass->Read(inGBufferData.mRenderTexture);

            auto state = inDevice.CreatePipelineStateDesc(inRenderPass, "blitVS", "pbrPS");
            state.InputLayout = {}; // clear the input layout, we generate the fullscreen triangle inside the vertex shader
            state.DepthStencilState.DepthEnable = FALSE;
            state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

            inDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&inData.mPipeline));
        },
        [&](const LightingData& inData, CommandList& inCmdList)
        {
            struct {
                uint32_t mShadowMaskTexture;
                uint32_t mGbufferDepthTexture;
                uint32_t mGbufferRenderTexture;
                uint32_t pad0;

            } root_constants;

            inCmdList->SetPipelineState(inData.mPipeline.Get());
            inCmdList.SetViewportScissorRect(inRenderGraph.GetViewport());
            inCmdList->SetGraphicsRoot32BitConstants(0, sizeof(root_constants) / sizeof(DWORD), &root_constants, 0);
            inCmdList->DrawInstanced(6, 1, 0, 0);
        });
}



const ComposeData& AddComposePass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferData& inGBufferData, TextureID inBackBuffer) {
    return inRenderGraph.AddGraphicsPass<ComposeData>("Compose Pass", inDevice,
        [&](IRenderPass* inRenderPass, ComposeData& inData)
        {
            inData.mInputTexture = inRenderPass->Read(inGBufferData.mRenderTexture); // Read will create an SRV for it
            inData.mOutputTexture = inRenderPass->Write(inBackBuffer); // backbuffer created with RTV, so Write just returns that

            auto state = inDevice.CreatePipelineStateDesc(inRenderPass, "blitVS", "blitPS");
            state.InputLayout = {}; // clear the input layout, we generate the fullscreen triangle inside the vertex shader
            state.DepthStencilState.DepthEnable = FALSE;
            state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

            inDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&inData.mPipeline));
        },

        [&](const ComposeData& inData, CommandList& inCmdList) 
        {
            auto input_texture = inDevice.GetTexture(inData.mInputTexture).GetHeapIndex();

            {   // manual barriers around the imported backbuffer resource, the rendergraph doesn't handle this kind of state
                auto backbuffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetResourcePtr(inData.mOutputTexture), D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
                inCmdList->ResourceBarrier(1, &backbuffer_barrier);
            }

            inCmdList->SetPipelineState(inData.mPipeline.Get());
            inCmdList.SetViewportScissorRect(inRenderGraph.GetViewport());
            inCmdList->SetGraphicsRoot32BitConstants(0, sizeof(input_texture) / sizeof(DWORD), &input_texture, 0);
            inCmdList->DrawInstanced(6, 1, 0, 0);

            {
                auto backbuffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetResourcePtr(inData.mOutputTexture), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON);
                inCmdList->ResourceBarrier(1, &backbuffer_barrier);
            }
        });
}



void AddFsrPass(RenderGraph& inRenderGraph, Device& inDevice) {
    //auto fsr2_dispatch_desc = FfxFsr2DispatchDescription();
  //fsr2_dispatch_desc.commandList              = ffxGetCommandListDX12(backbuffer_data.mCmdList.Get());
  //fsr2_dispatch_desc.color                    = ffxGetResourceDX12(&m_Fsr2, m_Device.GetResourcePtr(m_GBuffer.m_RenderTargetSRV),  L"Pre-Blit m_GBuffer.m_RenderTarget");
  //fsr2_dispatch_desc.depth                    = ffxGetResourceDX12(&m_Fsr2, m_Device.GetResourcePtr(m_GBuffer.m_DepthStencilSRV),  L"Pre-Blit m_GBuffer.m_DepthStencil");
  //fsr2_dispatch_desc.motionVectors            = ffxGetResourceDX12(&m_Fsr2, m_Device.GetResourcePtr(m_GBuffer.m_MotionVectorsSRV), L"Pre-Blit m_GBuffer.m_MotionVectors");
  //fsr2_dispatch_desc.output                   = ffxGetResourceDX12(&m_Fsr2, m_Device.GetResourcePtr(m_FsrOutputTexture),           L"FSR2-Output");
  //fsr2_dispatch_desc.exposure                 = ffxGetResourceDX12(&m_Fsr2, nullptr,                                               L"FSR2-Exposure");
  //fsr2_dispatch_desc.motionVectorScale.x      = float(m_Viewport.size.x);
  //fsr2_dispatch_desc.motionVectorScale.y      = float(m_Viewport.size.y);
  //fsr2_dispatch_desc.reset                    = false;
  //fsr2_dispatch_desc.enableSharpening         = false;
  //fsr2_dispatch_desc.sharpness                = 0.0f;
  //fsr2_dispatch_desc.frameTimeDelta           = Timer::sToMilliseconds(inDeltaTime);
  //fsr2_dispatch_desc.preExposure              = 1.0f;
  //fsr2_dispatch_desc.renderSize.width         = m_Viewport.size.x;
  //fsr2_dispatch_desc.renderSize.height        = m_Viewport.size.y;
  //fsr2_dispatch_desc.cameraFar                = m_Viewport.GetCamera().GetFar();
  //fsr2_dispatch_desc.cameraNear               = m_Viewport.GetCamera().GetNear();
  //fsr2_dispatch_desc.cameraFovAngleVertical   = glm::radians(m_Viewport.GetFov());

  //const auto jitter_phase_count = ffxFsr2GetJitterPhaseCount(m_Viewport.size.x, m_Viewport.size.x);
  //ffxFsr2GetJitterOffset(&fsr2_dispatch_desc.jitterOffset.x, &fsr2_dispatch_desc.jitterOffset.y, m_FsrFrameCounter, jitter_phase_count);
  //m_FsrFrameCounter = (m_FsrFrameCounter + 1) % jitter_phase_count;

  //gThrowIfFailed(ffxFsr2ContextDispatch(&m_Fsr2, &fsr2_dispatch_desc));

  // TODO: ImGui's DX12 impl is sub-optimal and costs over half a millisecond, should integrate it ourselves. see Vulkan back-end for impl
  // ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), backbuffer_data.mCmdList.Get());
}

} // raekor
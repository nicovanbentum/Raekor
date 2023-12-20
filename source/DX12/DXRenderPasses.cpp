#include "pch.h"
#include "DXRenderPasses.h"
#include "DXShader.h"
#include "Raekor/timer.h"
#include "Raekor/primitives.h"

namespace Raekor::DX12 {

RTTI_DEFINE_TYPE(SkyCubeData)               {}
RTTI_DEFINE_TYPE(ConvolveCubeData)          {}
RTTI_DEFINE_TYPE(GBufferData)               {}
RTTI_DEFINE_TYPE(GBufferDebugData)          {}
RTTI_DEFINE_TYPE(GrassData)                 {}
RTTI_DEFINE_TYPE(RTShadowMaskData)          {}
RTTI_DEFINE_TYPE(RTAOData)                  {}
RTTI_DEFINE_TYPE(ReflectionsData)           {}
RTTI_DEFINE_TYPE(DownsampleData)            {}
RTTI_DEFINE_TYPE(LightingData)              {}
RTTI_DEFINE_TYPE(FSR2Data)                  {}
RTTI_DEFINE_TYPE(XeSSData)                  {}
RTTI_DEFINE_TYPE(DLSSData)                  {}
RTTI_DEFINE_TYPE(ProbeTraceData)            {}
RTTI_DEFINE_TYPE(ProbeUpdateData)           {}
RTTI_DEFINE_TYPE(PathTraceData)             {}
RTTI_DEFINE_TYPE(ProbeDebugData)            {}
RTTI_DEFINE_TYPE(MeshletsRasterData)        {}
RTTI_DEFINE_TYPE(DebugLinesData)            {}
RTTI_DEFINE_TYPE(TAAResolveData)            {}
RTTI_DEFINE_TYPE(DepthOfFieldData)          {}
RTTI_DEFINE_TYPE(ComposeData)               {}
RTTI_DEFINE_TYPE(PreImGuiData)              {}
RTTI_DEFINE_TYPE(ImGuiData)                 {}
RTTI_DEFINE_TYPE(BloomDownscaleData)        {}
RTTI_DEFINE_TYPE(BloomData)                 {}
RTTI_DEFINE_TYPE(ShadowTraceData)           {}
RTTI_DEFINE_TYPE(ClassifyShadowTilesData)   {}
RTTI_DEFINE_TYPE(DenoiseShadowsData)        {}
RTTI_DEFINE_TYPE(ClearBufferData)           {}
RTTI_DEFINE_TYPE(ClearTextureFloatData)     {}

/*

const T& AddPass(RenderGraph& inRenderGraph, Device& inDevice)
{
    return inRenderGraph.AddGraphicsPass<T>("pass_name",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, T& inData)
    {
    },
    [](T& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
    });
}

*/



const ClearBufferData& AddClearBufferPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inBuffer, uint32_t inClearValue)
{
    return inRenderGraph.AddComputePass<ClearBufferData>(std::format("CLEAR BUFFER"),
        [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ClearBufferData& inData)
    {
        inData.mBufferUAV = ioRGBuilder.Write(inBuffer);
    },
        [&inDevice, inClearValue](ClearBufferData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        inCmdList.PushComputeConstants(ClearBufferRootConstants
        {
            .mClearValue = inClearValue,
            .mBuffer = inDevice.GetBindlessHeapIndex(inResources.GetBufferView(inData.mBufferUAV))
        });

        const auto& buffer = inDevice.GetBuffer(inResources.GetBufferView(inData.mBufferUAV));

        inCmdList->SetPipelineState(g_SystemShaders.mClearTextureShader.GetComputePSO());
        inCmdList->Dispatch(( buffer.GetSize() + 63 ) / 64, 1, 1);
    });
}



const ClearTextureFloatData& AddClearTextureFloatPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inTexture, const Vec4& inClearValue)
{
    return inRenderGraph.AddComputePass<ClearTextureFloatData>(std::format("CLEAR TEXTURE FLOAT"),
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ClearTextureFloatData& inData)
    {
        inData.mTextureUAV = ioRGBuilder.Write(inTexture);
    },
    [&inDevice, inClearValue](ClearTextureFloatData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        inCmdList.PushComputeConstants(ClearTextureRootConstants
        {
            .mClearValue = inClearValue,
            .mTexture = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mTextureUAV))
        });

        const auto& texture = inDevice.GetTexture(inResources.GetTextureView(inData.mTextureUAV));

        inCmdList->SetPipelineState(g_SystemShaders.mClearTextureShader.GetComputePSO());
        inCmdList->Dispatch(( texture.GetWidth() + 7 ) / 8, ( texture.GetHeight() + 7 ) / 8, 1);
    });
}



const SkyCubeData& AddSkyCubePass(RenderGraph& inRenderGraph, Device& inDevice, const Scene& inScene, TextureID inSkyCubeTexture)
{
    return inRenderGraph.AddComputePass<SkyCubeData>("SKY CUBE PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, SkyCubeData& inData)
    {  
        inData.mSkyCubeTexture = ioRGBuilder.Write(ioRGBuilder.Import(inDevice, inSkyCubeTexture));
    },

    [=, &inDevice, &inScene](SkyCubeData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {   
        if (auto sun_light = inScene.GetSunLight())
        {
            inCmdList.PushComputeConstants(SkyCubeRootConstants
            {
                .mSkyCubeTexture    = inDevice.GetBindlessHeapIndex(inResources.GetTexture(inData.mSkyCubeTexture)),
                .mSunLightDirection = sun_light->GetDirection(),
                .mSunLightColor     = sun_light->GetColor()
            });

            inCmdList->SetPipelineState(g_SystemShaders.mSkyCubeShader.GetComputePSO());

            const auto& texture_desc = inDevice.GetTexture(inResources.GetTexture(inData.mSkyCubeTexture)).GetDesc();

            inCmdList->Dispatch(texture_desc.width / 8, texture_desc.height / 8, texture_desc.depthOrArrayLayers);
        }
    });
}



const ConvolveCubeData& AddConvolveCubePass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inCubeTexture, TextureID inConvolvedCubeTexture)
{
    return inRenderGraph.AddGraphicsPass<ConvolveCubeData>("CONVOLVE CUBE PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ConvolveCubeData& inData)
    {
        inData.mCubeTexture = ioRGBuilder.Read(inCubeTexture);
        inData.mConvolvedCubeTexture = ioRGBuilder.Write(ioRGBuilder.Import(inDevice, inConvolvedCubeTexture));
    },

    [=, &inDevice](ConvolveCubeData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        inCmdList.PushComputeConstants(ConvolveCubeRootConstants
            {
                .mCubeTexture = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mCubeTexture)),
                .mConvolvedCubeTexture = inDevice.GetBindlessHeapIndex(inResources.GetTexture(inData.mConvolvedCubeTexture))
            });

        inCmdList->SetPipelineState(g_SystemShaders.mConvolveCubeShader.GetComputePSO());

        const auto& texture_desc = inDevice.GetTexture(inResources.GetTexture(inData.mConvolvedCubeTexture)).GetDesc();

        inCmdList->Dispatch(texture_desc.width / 8, texture_desc.height / 8, texture_desc.depthOrArrayLayers);
    });
}



const MeshletsRasterData& AddMeshletsRasterPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene)
{
    return inRenderGraph.AddGraphicsPass<MeshletsRasterData>("MESHLETS RASTER PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, MeshletsRasterData& inData)
    {
        inData.mRenderTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = "GBUFFER RENDER"
        });

        inData.mVelocityTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = "GBUFFER VELOCITY"
        });

        inData.mDepthTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::DEPTH_STENCIL_TARGET,
            .debugName = "GBUFFER DEPTH"
        });

        ioRGBuilder.RenderTarget(inData.mRenderTexture); // SV_Target0
        ioRGBuilder.RenderTarget(inData.mVelocityTexture); // SV_Target1
        ioRGBuilder.DepthStencilTarget(inData.mDepthTexture);

        CD3DX12_SHADER_BYTECODE vertex_shader, pixel_shader;
        g_SystemShaders.mGBufferShader.GetGraphicsProgram(vertex_shader, pixel_shader);

        auto pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, vertex_shader, pixel_shader);
        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(inData.mPipeline.GetAddressOf()));
        inData.mPipeline->SetName(L"PSO_MESHLETS_RASTER");
    },

    [&inRenderGraph, &inDevice, &inScene](MeshletsRasterData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        const auto& viewport = inRenderGraph.GetViewport();
        inCmdList.SetViewportAndScissor(viewport);
        inCmdList->SetPipelineState(inData.mPipeline.Get());

        const auto clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        inCmdList->ClearDepthStencilView(inDevice.GetCPUDescriptorHandle(inResources.GetTexture(inData.mDepthTexture)), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
        inCmdList->ClearRenderTargetView(inDevice.GetCPUDescriptorHandle(inResources.GetTexture(inData.mRenderTexture)), glm::value_ptr(clear_color), 0, nullptr);
        inCmdList->ClearRenderTargetView(inDevice.GetCPUDescriptorHandle(inResources.GetTexture(inData.mVelocityTexture)), glm::value_ptr(clear_color), 0, nullptr);

        for (const auto& [entity, mesh] : inScene->Each<Mesh>())
        {
            auto transform = inScene->GetPtr<Transform>(entity);
            if (!transform)
                continue;

            if (!BufferID(mesh.BottomLevelAS).IsValid())
                continue;

            if (mesh.meshlets.empty())
                continue;

            auto material = inScene->GetPtr<Material>(mesh.material);
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

            auto material_index = inScene->GetSparseIndex<Material>(mesh.material);
            material_index = material_index == UINT32_MAX ? 0 : material_index;

            const auto instance_index = inScene->GetSparseIndex<Mesh>(entity);

            auto root_constants = GbufferRootConstants {
                .mVertexBuffer       = inDevice.GetBindlessHeapIndex(BufferID(mesh.vertexBuffer)),
                .mInstancesBuffer    = inDevice.GetBindlessHeapIndex(inScene.GetInstancesDescriptor(inDevice)),
                .mMaterialsBuffer    = inDevice.GetBindlessHeapIndex(inScene.GetMaterialsDescriptor(inDevice)),
                .mInstanceIndex      = instance_index,
            };

            inCmdList->SetGraphicsRoot32BitConstants(0, sizeof(root_constants) / sizeof(DWORD), &root_constants, 0);

            inCmdList->IASetIndexBuffer(&index_view);

            if (entity == inData.mActiveEntity)
            {
                // do stencil stuff?
            }

            inCmdList->DrawIndexedInstanced(mesh.indices.size(), 1, 0, 0, 0);
        }

        // std::cout << std::format("gbuffer pass took {:.2f} ms.\n", Timer::sToMilliseconds(timer.Restart()));
    });
}



const GBufferData& AddGBufferPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene)
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
            .debugName = "GBUFFER RENDER"
        });

        inData.mVelocityTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = "GBUFFER VELOCITY"
        });

        inData.mDepthTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::DEPTH_STENCIL_TARGET,
            .debugName = "GBUFFER DEPTH"
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

        Timer timer;

        for (const auto& [entity, mesh] : inScene->Each<Mesh>())
        {
            auto transform = inScene->GetPtr<Transform>(entity);
            if (!transform)
                continue;

            if (!BufferID(mesh.BottomLevelAS).IsValid())
                continue;

            if (!mesh.meshlets.empty())
                continue;

            auto material = inScene->GetPtr<Material>(mesh.material);
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

            auto material_index = inScene->GetSparseIndex<Material>(mesh.material);
            material_index = material_index == UINT32_MAX ? 0 : material_index;

            const auto instance_index = inScene->GetSparseIndex<Mesh>(entity);

            auto root_constants = GbufferRootConstants {
                .mVertexBuffer       = inDevice.GetBindlessHeapIndex(BufferID(mesh.vertexBuffer)),
                .mInstancesBuffer    = inDevice.GetBindlessHeapIndex(inScene.GetInstancesDescriptor(inDevice)),
                .mMaterialsBuffer    = inDevice.GetBindlessHeapIndex(inScene.GetMaterialsDescriptor(inDevice)),
                .mInstanceIndex      = instance_index,
            };

            inCmdList->SetGraphicsRoot32BitConstants(0, sizeof(root_constants) / sizeof(DWORD), &root_constants, 0);

            inCmdList->IASetIndexBuffer(&index_view);

            if (entity == inData.mActiveEntity)
            {
                // do stencil stuff?
            }

            inCmdList->DrawIndexedInstanced(mesh.indices.size(), 1, 0, 0, 0);
        }

        // std::cout << std::format("gbuffer pass took {:.2f} ms.\n", Timer::sToMilliseconds(timer.Restart()));
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
            .debugName = "GBUFFER_DEBUG"
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



const RenderGraphResourceID AddRayTracedShadowsPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferData& inGBufferData)
{
    constexpr auto cTileSize = 16u;
    constexpr auto cPackedRaysWidth = 8u;
    constexpr auto cPackedRaysHeight = 4u;

    auto TraceShadowRaysPass = [](RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferData& inGBufferData)
    {
        return inRenderGraph.AddComputePass<ShadowTraceData>("TRACE SHADOW RAYS PASS",
        [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, ShadowTraceData& inData)
        {
            inData.mOutputTexture = inRGBuilder.Create(Texture::Desc
            {
                .format = DXGI_FORMAT_R32_UINT,
                .width  = (inRenderGraph.GetViewport().size.x + cPackedRaysWidth - 1 ) / cPackedRaysWidth,
                .height = (inRenderGraph.GetViewport().size.y + cPackedRaysHeight  - 1 ) / cPackedRaysHeight,
                .usage  = Texture::Usage::SHADER_READ_WRITE,
                .debugName = "PACKED SHADOW RAY HITS"
            });

            inRGBuilder.Write(inData.mOutputTexture);
            inData.mGBufferDepthTextureSRV = inRGBuilder.Read(inGBufferData.mDepthTexture);
            inData.mGBufferRenderTextureSRV = inRGBuilder.Read(inGBufferData.mRenderTexture);
        },

        [&inRenderGraph, &inDevice, &inScene](ShadowTraceData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
        {
            if (inScene->Count<Mesh>() == 0)
                return;

            auto& viewport = inRenderGraph.GetViewport();
            auto& texture  = inDevice.GetTexture(inResources.GetTexture(inData.mOutputTexture));

            inCmdList.PushComputeConstants(ShadowMaskRootConstants
            {
                .mShadowMaskTexture = inDevice.GetBindlessHeapIndex(inResources.GetTexture(inData.mOutputTexture)),
                .mGbufferDepthTexture = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mGBufferDepthTextureSRV)),
                .mGbufferRenderTexture = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mGBufferRenderTextureSRV)),
                .mTLAS = inDevice.GetBindlessHeapIndex(inScene.GetTLASDescriptor(inDevice)),
                .mDispatchSize = viewport.GetRenderSize()
            });

            inCmdList->SetPipelineState(g_SystemShaders.mTraceShadowRaysShader.GetComputePSO());
            inCmdList->Dispatch(texture.GetWidth(), texture.GetHeight(), 1);
        });
    };

    auto ClassifyShadowTilesPass = [](RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const ShadowTraceData& inTraceData)
    {
        return inRenderGraph.AddComputePass<ClassifyShadowTilesData>("CLASSIFY SHADOW RAYS PASS",
        [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, ClassifyShadowTilesData& inData)
        {
            const auto& render_size = inRenderGraph.GetViewport().GetRenderSize();
            const auto tile_width   = ( render_size.x + cTileSize - 1 ) / cTileSize;
            const auto tile_height  = ( render_size.y + cTileSize - 1 ) / cTileSize;

            inData.mTilesBuffer = inRGBuilder.Create(Buffer::Desc 
            { 
                .format = DXGI_FORMAT_R32_UINT, 
                .size   = tile_width * tile_height * sizeof(uint32_t), 
                .usage  = Buffer::SHADER_READ_WRITE,
                .debugName = "SHADOW_TILES_BUFFER"
            });

            inData.mIndirectDispatchBuffer = inRGBuilder.Create(Buffer::Desc 
            { 
                .format = DXGI_FORMAT_UNKNOWN, 
                .size   = sizeof(D3D12_DISPATCH_ARGUMENTS), 
                .usage  = Buffer::SHADER_READ_WRITE,
                .debugName = "SHADOWS_DISPATCH_BUFFER"
            });

            inData.mTracedShadowRaysTextureSRV = inRGBuilder.Read(inTraceData.mOutputTexture);
        },

        [&inRenderGraph, &inDevice, &inScene](ClassifyShadowTilesData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
        {
            inCmdList.PushComputeConstants(ClearBufferRootConstants
            {
                .mBuffer = inDevice.GetBindlessHeapIndex(inResources.GetBuffer(inData.mTilesBuffer))
            });

            const auto& tiles_buffer = inDevice.GetBuffer(inResources.GetBuffer(inData.mTilesBuffer));

            inCmdList->SetPipelineState(g_SystemShaders.mClearBufferShader.GetComputePSO());
            inCmdList->Dispatch(( tiles_buffer.GetSize() + 63 ) / 64, 1, 1);

            {
                const auto uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(tiles_buffer.GetD3D12Resource().Get());
                inCmdList->ResourceBarrier(1, &uav_barrier);
            }

            inCmdList.PushComputeConstants(ClearBufferRootConstants
            {
                .mBuffer = inDevice.GetBindlessHeapIndex(inResources.GetBuffer(inData.mIndirectDispatchBuffer))
            });

            const auto& dispatch_buffer = inDevice.GetBuffer(inResources.GetBuffer(inData.mIndirectDispatchBuffer));

            inCmdList->SetPipelineState(g_SystemShaders.mClearBufferShader.GetComputePSO());
            inCmdList->Dispatch(( dispatch_buffer.GetSize() + 63 ) / 64, 1, 1);

            {
                const auto uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(dispatch_buffer.GetD3D12Resource().Get());
                inCmdList->ResourceBarrier(1, &uav_barrier);
            }

            const auto& viewport = inRenderGraph.GetViewport();
            const auto  dispatch_size = UVec2(( viewport.size.x + cTileSize - 1 ) / cTileSize, ( viewport.size.y + cTileSize - 1 ) / cTileSize);


            inCmdList.PushComputeConstants(ShadowsClassifyRootConstants 
            { 
                .mShadowMaskTexture = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mTracedShadowRaysTextureSRV)),
                .mTilesBuffer       = inDevice.GetBindlessHeapIndex(inResources.GetBuffer(inData.mTilesBuffer)),
                .mDispatchBuffer    = inDevice.GetBindlessHeapIndex(inResources.GetBuffer(inData.mIndirectDispatchBuffer)),
                .mDispatchSize      = viewport.size
            });

            inCmdList->SetPipelineState(g_SystemShaders.mClassifyShadowTilesShader.GetComputePSO());
            inCmdList->Dispatch(dispatch_size.x, dispatch_size.y, 1);

            {
                const auto uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(dispatch_buffer.GetD3D12Resource().Get());
                inCmdList->ResourceBarrier(1, &uav_barrier);
            }

            {
                const auto uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(tiles_buffer.GetD3D12Resource().Get());
                inCmdList->ResourceBarrier(1, &uav_barrier);
            }
        });
    };

    auto DenoiseShadowsPass = [](RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const ShadowTraceData& inTraceData, const ClassifyShadowTilesData& inClassifyData)
    {
        return inRenderGraph.AddComputePass<DenoiseShadowsData>("DENOISE SHADOWS PASS",
        [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, DenoiseShadowsData& inData)
        {
            inData.mOutputTexture = inRGBuilder.Create(Texture::Desc
            {
                .format = DXGI_FORMAT_R32G32_FLOAT,
                .width  = inRenderGraph.GetViewport().size.x,
                .height = inRenderGraph.GetViewport().size.y,
                .usage  = Texture::Usage::SHADER_READ_WRITE,
                .debugName = "RAY TRACED SHADOW MASK"
            });

            inRGBuilder.Write(inData.mOutputTexture);

            inData.mTracedShadowRaysTextureSRV = inRGBuilder.Read(inTraceData.mOutputTexture);

            inData.mTilesBufferSRV = inRGBuilder.Read(inClassifyData.mTilesBuffer);
            inData.mDenoisedTilesBufferSRV = inRGBuilder.Read(inClassifyData.mTilesBuffer);

            // don't actually need the views, just barriers and render graph resource IDs
            inData.mIndirectDispatchBufferSRV = inRGBuilder.Read(inClassifyData.mIndirectDispatchBuffer);
            inData.mDenoisedIndirectDispatchBufferSRV = inRGBuilder.Read(inClassifyData.mIndirectDispatchBuffer);

            // clear the final shadow texture to white (nothing in shadow)
            // AddClearTextureFloatPass(inRenderGraph, inDevice, inData.mOutputTexture, Vec4(1.0f, 1.0f, 1.0f, 1.0f));
        },

        [&inRenderGraph, &inDevice, &inScene](DenoiseShadowsData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
        {
            inCmdList.PushComputeConstants(ClearTextureRootConstants
            {
                .mClearValue = Vec4(1.0f, 1.0f, 1.0f, 1.0f),
                .mTexture = inDevice.GetBindlessHeapIndex(inResources.GetTexture(inData.mOutputTexture))
            });

            const auto& texture = inDevice.GetTexture(inResources.GetTexture(inData.mOutputTexture));

            inCmdList->SetPipelineState(g_SystemShaders.mClearTextureShader.GetComputePSO());
            inCmdList->Dispatch(( texture.GetWidth() + 7 ) / 8, ( texture.GetHeight() + 7 ) / 8, 1);

            const auto uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(texture.GetD3D12Resource().Get());
            inCmdList->ResourceBarrier(1, &uav_barrier);

            auto root_constants = ShadowsDenoiseRootConstants
            {
                .mResultTexture     = inDevice.GetBindlessHeapIndex(inResources.GetTexture(inData.mOutputTexture)),
                .mShadowMaskTexture = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mTracedShadowRaysTextureSRV))
            };

            //{
            //    // write black to the final shadow texture for all the tiles that didn't need denoising
            //    auto dispatch_buffer = inResources.GetBuffer(inData.mIndirectDispatchBufferSRV);
            //    inCmdList->ExecuteIndirect(inDevice.GetCommandSignature(COMMAND_SIGNATURE_DISPATCH), 1, inDevice.GetD3D12Resource(dispatch_buffer), 0, nullptr, 0);
            //}

            {
                // write all the denoised tiles to the final shadow texture
                root_constants.mTilesBuffer = inDevice.GetBindlessHeapIndex(inResources.GetBufferView(inData.mDenoisedTilesBufferSRV));
                inCmdList.PushComputeConstants(root_constants);

                inCmdList->SetPipelineState(g_SystemShaders.mDenoiseShadowTilesShader.GetComputePSO());

                auto dispatch_buffer = inResources.GetBufferView(inData.mDenoisedIndirectDispatchBufferSRV);
                inCmdList->ExecuteIndirect(inDevice.GetCommandSignature(COMMAND_SIGNATURE_DISPATCH), 1, inDevice.GetD3D12Resource(dispatch_buffer), 0, nullptr, 0);
            }

        });
    };

    const auto& traced_rays_data    = TraceShadowRaysPass(inRenderGraph, inDevice, inScene, inGBufferData);

    const auto& classify_tiles_data = ClassifyShadowTilesPass(inRenderGraph, inDevice, inScene, traced_rays_data);

    const auto& denoise_tiles_data  = DenoiseShadowsPass(inRenderGraph, inDevice, inScene, traced_rays_data, classify_tiles_data);

    return denoise_tiles_data.mOutputTexture;
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
            .debugName = "RAY TRACED SHADOW MASK"
        });

        inRGBuilder.Write(inData.mOutputTexture);

        inData.mGbufferDepthTextureSRV = inRGBuilder.Read(inGBufferData.mDepthTexture);
        inData.mGBufferRenderTextureSRV = inRGBuilder.Read(inGBufferData.mRenderTexture);
    },

    [&inRenderGraph, &inDevice, &inScene](RTShadowMaskData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        if (inScene->Count<Mesh>() == 0)
            return;

        auto& viewport = inRenderGraph.GetViewport();

        inCmdList.PushComputeConstants(ShadowMaskRootConstants
        {
            .mShadowMaskTexture    = inDevice.GetBindlessHeapIndex(inResources.GetTexture(inData.mOutputTexture)),
            .mGbufferDepthTexture  = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mGbufferDepthTextureSRV)),
            .mGbufferRenderTexture = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mGBufferRenderTextureSRV)),
            .mTLAS                 = inDevice.GetBindlessHeapIndex(inScene.GetTLASDescriptor(inDevice)),
            .mDispatchSize         = viewport.size
        });

        inCmdList->SetPipelineState(g_SystemShaders.mRTShadowsShader.GetComputePSO());
        inCmdList->Dispatch((viewport.size.x + 7) / 8, (viewport.size.y + 7) / 8, 1);
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
            .debugName = "RAY TRACED AO MASK"
        });

        inRGBuilder.Write(inData.mOutputTexture);

        inData.mGbufferDepthTextureSRV = inRGBuilder.Read(inGbufferData.mDepthTexture);
        inData.mGBufferRenderTextureSRV = inRGBuilder.Read(inGbufferData.mRenderTexture);
    },

    [&inRenderGraph, &inDevice, &inScene](RTAOData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        if (inScene->Count<Mesh>() == 0)
            return;

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

        inCmdList->SetPipelineState(g_SystemShaders.mRTAmbientOcclusionShader.GetComputePSO());
        inCmdList->Dispatch((viewport.size.x + 7) / 8, (viewport.size.y + 7) / 8, 1);
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
    return inRenderGraph.AddComputePass<ReflectionsData>("RAY TRACED REFLECTIONS PASS",
    [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, ReflectionsData& inData)
    {
        inData.mOutputTexture = inRGBuilder.Create(Texture::Desc
        {
            .format     = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width      = inRenderGraph.GetViewport().size.x,
            .height     = inRenderGraph.GetViewport().size.y,
            .mipLevels  = 0, // let it auto-calculate the nr of mips
            .usage      = Texture::Usage::SHADER_READ_WRITE,
            .debugName = "RAY TRACED REFLECTIONS"
        });

        inRGBuilder.Write(inData.mOutputTexture);

        inData.mGBufferDepthTextureSRV = inRGBuilder.Read(inGBufferData.mDepthTexture);
        inData.mGbufferRenderTextureSRV = inRGBuilder.Read(inGBufferData.mRenderTexture);
    },

    [&inRenderGraph, &inDevice, &inScene](ReflectionsData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        if (inScene->Count<Mesh>() == 0)
            return;

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

        inCmdList->SetPipelineState(g_SystemShaders.mRTReflectionsShader.GetComputePSO());
        inCmdList->Dispatch((viewport.size.x + 7) / 8, (viewport.size.y + 7) / 8, 1);
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
            .debugName = "RT_PATH_TRACE_RESULT"
        });

        inData.mAccumulationTexture = inRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = "RT_PATH_TRACE_ACCUMULATION"
        });

        inRGBuilder.Write(inData.mOutputTexture);
        inRGBuilder.Write(inData.mAccumulationTexture);
    },

    [&inRenderGraph, &inDevice, &inScene](PathTraceData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        auto& viewport = inRenderGraph.GetViewport();

        auto constants = PathTraceRootConstants
        {
            .mTLAS                  = inDevice.GetBindlessHeapIndex(inScene.GetTLASDescriptor(inDevice)),
            .mBounces               = inData.mBounces,
            .mInstancesBuffer       = inDevice.GetBindlessHeapIndex(inScene.GetInstancesDescriptor(inDevice)),
            .mMaterialsBuffer       = inDevice.GetBindlessHeapIndex(inScene.GetMaterialsDescriptor(inDevice)),
            .mResultTexture         = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mOutputTexture)),
            .mAccumulationTexture   = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mAccumulationTexture)),
            .mDispatchSize          = viewport.size,
            .mReset                 = PathTraceData::mReset,
        };

        constants.mLightsCount = inScene->Count<Light>();

        if (constants.mLightsCount > 0)
            constants.mLightsBuffer = inDevice.GetBindlessHeapIndex(inScene.GetLightsDescriptor(inDevice));

        inCmdList.PushComputeConstants(constants);

        inCmdList->SetPipelineState(g_SystemShaders.mRTPathTraceShader.GetComputePSO());
        inCmdList->Dispatch((viewport.size.x + 7) / 8, (viewport.size.y + 7) / 8, 1);

        PathTraceData::mReset = false;
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
            .debugName = "SPD_ATOMIC_UINT_BUFFER"
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
            .debugName = "DDGI TRACE DEPTH"
        });

        inData.mRaysIrradianceTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R11G11B10_FLOAT,
            .width  = DDGI_RAYS_PER_PROBE,
            .height = uint32_t(total_probe_count),
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = "DDGI TRACE IRRADIANCE"
        });

        ioRGBuilder.Write(inData.mRaysDepthTexture);
        ioRGBuilder.Write(inData.mRaysIrradianceTexture);
    },

    [&inRenderGraph, &inDevice, &inScene](ProbeTraceData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        if (inScene->Count<Mesh>() == 0)
            return;

        auto Index3Dto1D = [](UVec3 inIndex, UVec3 inCount)
        {
            return inIndex.x + inIndex.y * inCount.x + inIndex.z * inCount.x * inCount.y;
        };

        if (inScene->Any<DDGISceneSettings>() && inScene->Count<DDGISceneSettings>())
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
            .mRandomRotationMatrix = inData.mRandomRotationMatrix,
            .mDDGIData = inData.mDDGIData
        });

        const auto total_probe_count = inData.mDDGIData.mProbeCount.x * inData.mDDGIData.mProbeCount.y * inData.mDDGIData.mProbeCount.z;

        inCmdList->SetPipelineState(g_SystemShaders.mProbeTraceShader.GetComputePSO());
        inCmdList->Dispatch(DDGI_RAYS_PER_PROBE / DDGI_TRACE_SIZE, total_probe_count, 1);
    });
}



const ProbeUpdateData& AddProbeUpdatePass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const ProbeTraceData& inTraceData)
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
            .debugName = "DDGI UPDATE DEPTH"
        });

        inData.mProbesIrradianceTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = uint32_t(DDGI_IRRADIANCE_TEXELS * DDGI_PROBES_PER_ROW),
            .height = uint32_t(DDGI_IRRADIANCE_TEXELS * ( total_probe_count / DDGI_PROBES_PER_ROW )),
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = "DDGI UPDATE IRRADIANCE"
        });

        ioRGBuilder.Write(inData.mProbesDepthTexture);
        ioRGBuilder.Write(inData.mProbesIrradianceTexture);

        inData.mRaysDepthTextureSRV = ioRGBuilder.Read(inTraceData.mRaysDepthTexture);
        inData.mRaysIrradianceTextureSRV = ioRGBuilder.Read(inTraceData.mRaysIrradianceTexture);
    },

    [&inDevice, &inTraceData, &inScene](ProbeUpdateData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        if (inScene->Count<Mesh>() == 0)
            return;

        inData.mDDGIData                            = inTraceData.mDDGIData;
        inData.mDDGIData.mRaysDepthTexture          = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mRaysDepthTextureSRV));
        inData.mDDGIData.mProbesDepthTexture        = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mProbesDepthTexture));
        inData.mDDGIData.mRaysIrradianceTexture     = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mRaysIrradianceTextureSRV));
        inData.mDDGIData.mProbesIrradianceTexture   = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mProbesIrradianceTexture));
        
        /*{
            inCmdList->SetPipelineState(g_SystemShaders.mProbeUpdateDepthShader.GetComputePSO());
            inCmdList.PushComputeConstants(ProbeUpdateRootConstants
            {
                .mRandomRotationMatrix = inTraceData.mRandomRotationMatrix,
                .mDDGIData = inData.mDDGIData
            });

            const auto depth_texture = inDevice.GetTexture(inRGResources.GetTexture(inData.mProbesDepthTexture));
            inCmdList->Dispatch(depth_texture.GetDesc().width / DDGI_DEPTH_TEXELS, depth_texture.GetDesc().height / DDGI_DEPTH_TEXELS, 1);
        }*/

        {
            inCmdList->SetPipelineState(g_SystemShaders.mProbeUpdateIrradianceShader.GetComputePSO());
            inCmdList.PushComputeConstants(ProbeUpdateRootConstants
            {
                .mRandomRotationMatrix = inTraceData.mRandomRotationMatrix,
                .mDDGIData = inData.mDDGIData
            });

            const auto& irradiance_texture = inDevice.GetTexture(inRGResources.GetTexture(inData.mProbesIrradianceTexture));
            inCmdList->Dispatch(irradiance_texture.GetDesc().width / DDGI_IRRADIANCE_TEXELS, irradiance_texture.GetDesc().height / DDGI_IRRADIANCE_TEXELS, 1);
        }


    });
}



const ProbeDebugData& AddProbeDebugPass(RenderGraph& inRenderGraph, Device& inDevice, const ProbeTraceData& inTraceData, const ProbeUpdateData& inUpdateData, RenderGraphResourceID inRenderTarget, RenderGraphResourceID inDepthTarget)
{
    return inRenderGraph.AddGraphicsPass<ProbeDebugData>("GI PROBE DEBUG PASS",

    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ProbeDebugData& inData)
    {
        gGenerateSphere(inData.mProbeMesh, 0.5f, 32u, 32u);

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
            .debugName = "DEBUG LINES VERTEX BUFFER"
        });


        inData.mIndirectArgsBuffer = ioRGBuilder.Create(Buffer::Desc
        {
            .size     = sizeof(D3D12_DRAW_ARGUMENTS),
            .usage    = Buffer::Usage::SHADER_READ_WRITE,
            // .viewDesc = args_srv_desc
            .debugName = "DEBUG LINES INDIRECT ARGS BUFFER"
        });

        // ioRGBuilder.Write(inData.mVertexBuffer);

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



const LightingData& AddLightingPass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferData& inGBufferData, RenderGraphResourceID inShadowTexture, const ReflectionsData& inReflectionsData, RenderGraphResourceID inAOTexture, const ProbeUpdateData& inProbeData)
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
            .debugName = "SHADING OUTPUT"
        });

        ioRGBuilder.RenderTarget(inData.mOutputTexture);

        inData.mDDGIData = inProbeData.mDDGIData;
        inData.mShadowMaskTextureSRV        = ioRGBuilder.Read(inShadowTexture);
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
            .width  = inRenderGraph.GetViewport().GetDisplaySize().x,
            .height = inRenderGraph.GetViewport().GetDisplaySize().y,
            .usage  = Texture::SHADER_READ_WRITE,
            .debugName = "FSR2_OUTPUT"
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
        fsr2_dispatch_desc.exposure = ffxGetResourceDX12(&inContext, nullptr);
        fsr2_dispatch_desc.output = ffxGetResourceDX12(&inContext, output_texture_ptr, nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
        fsr2_dispatch_desc.motionVectorScale.x = -float(viewport.GetRenderSize().x);
        fsr2_dispatch_desc.motionVectorScale.y = -float(viewport.GetRenderSize().y);
        fsr2_dispatch_desc.renderSize.width = viewport.GetRenderSize().x;
        fsr2_dispatch_desc.renderSize.height = viewport.GetRenderSize().y;
        fsr2_dispatch_desc.enableSharpening = false;
        fsr2_dispatch_desc.sharpness = 0.0f;
        fsr2_dispatch_desc.frameTimeDelta = Timer::sToMilliseconds(inData.mDeltaTime);
        fsr2_dispatch_desc.preExposure = 1.0f;
        fsr2_dispatch_desc.reset = false;
        fsr2_dispatch_desc.cameraNear = viewport.GetCamera().GetNear();
        fsr2_dispatch_desc.cameraFar = viewport.GetCamera().GetFar();
        fsr2_dispatch_desc.cameraFovAngleVertical = glm::radians(viewport.GetFieldOfView());

        const auto jitter_phase_count = ffxFsr2GetJitterPhaseCount(viewport.GetRenderSize().x, viewport.GetDisplaySize().x);
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
            .width  = inRenderGraph.GetViewport().GetDisplaySize().x,
            .height = inRenderGraph.GetViewport().GetDisplaySize().y,
            .usage  = Texture::SHADER_READ_WRITE,
            .debugName = "DLSS_OUTPUT"
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

        const auto color_texture_ptr  = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mColorTextureSRV));
        const auto depth_texture_ptr  = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mDepthTextureSRV));
        const auto movec_texture_ptr  = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mMotionVectorTextureSRV));
        const auto output_texture_ptr = inDevice.GetD3D12Resource(inResources.GetTexture(inData.mOutputTexture));
        
        NVSDK_NGX_D3D12_DLSS_Eval_Params eval_params = {};
        eval_params.Feature.pInColor = color_texture_ptr;
        eval_params.Feature.pInOutput = output_texture_ptr;
        eval_params.pInDepth = depth_texture_ptr;
        eval_params.pInMotionVectors = movec_texture_ptr;
        eval_params.InMVScaleX = -float(viewport.GetRenderSize().x);
        eval_params.InMVScaleY = -float(viewport.GetRenderSize().y);
        eval_params.InRenderSubrectDimensions.Width = viewport.GetRenderSize().x;
        eval_params.InRenderSubrectDimensions.Height = viewport.GetRenderSize().y;

        const auto jitter_phase_count = ffxFsr2GetJitterPhaseCount(viewport.GetRenderSize().x, viewport.GetDisplaySize().x);
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
            .width  = inRenderGraph.GetViewport().GetDisplaySize().x,
            .height = inRenderGraph.GetViewport().GetDisplaySize().y,
            .usage  = Texture::SHADER_READ_WRITE,
            .debugName = "DLSS_OUTPUT",
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
        exec_params.exposureScale    = 1.0f;
        exec_params.inputWidth       = viewport.GetRenderSize().x;
        exec_params.inputHeight      = viewport.GetRenderSize().y;

        const auto jitter_phase_count = ffxFsr2GetJitterPhaseCount(viewport.GetRenderSize().x, viewport.GetDisplaySize().x);
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
            .debugName = "TAA OUTPUT"
        });

        inData.mHistoryTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::SHADER_READ_ONLY,
            .debugName = "TAA HISTORY"
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
            .mRenderSize      = inRenderGraph.GetViewport().GetRenderSize(),
            .mRenderSizeRcp   = 1.0f / Vec2(inRenderGraph.GetViewport().GetRenderSize()),
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



const DepthOfFieldData& AddDepthOfFieldPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inInputTexture, RenderGraphResourceID inDepthTexture)
{
    return inRenderGraph.AddComputePass<DepthOfFieldData>("DEPTH OF FIELD PASS",

    [&](RenderGraphBuilder& inBuilder, IRenderPass* inRenderPass, DepthOfFieldData& inData)
    {
        inData.mOutputTexture = inBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().GetDisplaySize().x,
            .height = inRenderGraph.GetViewport().GetDisplaySize().y,
            .usage  = Texture::SHADER_READ_WRITE,
            .debugName = "DOF OUTPUT"
        });

        inBuilder.Write(inData.mOutputTexture);
        
        inData.mDepthTextureSRV = inBuilder.Read(inDepthTexture);
        inData.mInputTextureSRV = inBuilder.Read(inInputTexture);
    },

    [&inRenderGraph, &inDevice](DepthOfFieldData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        const auto& viewport = inRenderGraph.GetViewport();

        inCmdList.PushComputeConstants(DepthOfFieldRootConstants {
            .mDepthTexture  = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mDepthTextureSRV)),
            .mInputTexture  = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mInputTextureSRV)),
            .mOutputTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mOutputTexture)),
            .mFarPlane      = viewport.GetCamera().GetFar(),
            .mNearPlane     = viewport.GetCamera().GetNear(),
            .mFocusPoint    = inData.mFocusPoint,
            .mFocusScale    = inData.mFocusScale,
            .mDispatchSize  = viewport.size
        });

        inCmdList->SetPipelineState(g_SystemShaders.mDepthOfFieldShader.GetComputePSO());
        inCmdList->Dispatch((viewport.GetDisplaySize().x + 7) / 8, (viewport.GetDisplaySize().y + 7) / 8, 1);
    });
}



const BloomData& AddBloomPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inInputTexture)
{
    auto RunBloomPass = [&](const std::string& inPassName, ID3D12PipelineState* inPipeline, RenderGraphResourceID inFromTexture, RenderGraphResourceID inToTexture, uint32_t inFromMip, uint32_t inToMip)
    {
        return inRenderGraph.AddComputePass<BloomDownscaleData>(inPassName,
        [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, BloomDownscaleData& inData)
        {
            inData.mToTextureMip   = inToMip;
            inData.mFromTextureMip = inFromMip;
            inData.mToTextureUAV   = ioRGBuilder.WriteTexture(inToTexture, inToMip);
            inData.mFromTextureSRV = ioRGBuilder.ReadTexture(inFromTexture, inFromMip);
        },
        [&inRenderGraph, &inDevice, inPipeline](BloomDownscaleData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
        {
            const auto dest_viewport = CD3DX12_VIEWPORT(inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mToTextureUAV)), inData.mToTextureMip);
            
            inCmdList.PushComputeConstants(BloomRootConstants
            {
                .mSrcTexture   = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mFromTextureSRV)),
                .mSrcMipLevel  = inData.mFromTextureMip,
                .mDstTexture   = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mToTextureUAV)),
                .mDstMipLevel  = inData.mToTextureMip,
                .mDispatchSize = UVec2(dest_viewport.Width, dest_viewport.Height)
            });

            inCmdList->SetPipelineState(inPipeline);
            inCmdList->Dispatch(( dest_viewport.Width + 7 ) / 8, ( dest_viewport.Height + 7 ) / 8, 1);
        });
    };
    

    auto AddDownsamplePass = [&](RenderGraphResourceID inFromTexture, RenderGraphResourceID inToTexture, uint32_t inFromMip, uint32_t inToMip)
    {
        return RunBloomPass(std::format("Downsample Mip {} -> Mip {}", inFromMip, inToMip), g_SystemShaders.mBloomDownsampleShader.GetComputePSO(), inFromTexture, inToTexture, inFromMip, inToMip);
    };

    auto AddUpsamplePass = [&](RenderGraphResourceID inFromTexture, RenderGraphResourceID inToTexture, uint32_t inFromMip, uint32_t inToMip)
    {
        return RunBloomPass(std::format("Upsample Mip {} -> Mip {}", inFromMip, inToMip), g_SystemShaders.mBloomUpSampleShader.GetComputePSO(), inFromTexture, inToTexture, inFromMip, inToMip);
    };


    return inRenderGraph.AddComputePass<BloomData>("BLOOM PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, BloomData& inData)
    {
        const auto mip_levels = 6u;

        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format    = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width     = inRenderGraph.GetViewport().size.x,
            .height    = inRenderGraph.GetViewport().size.y,
            .mipLevels = mip_levels,
            .usage     = Texture::SHADER_READ_WRITE,
            .debugName = "BLOOM OUTPUT"
        });

        ioRGBuilder.Write(inData.mOutputTexture);

        // do an initial downsample from the current final scene texture to the first mip of the bloom chain
        AddDownsamplePass(inInputTexture, inData.mOutputTexture, 0, 1);

        // downsample along the bloom chain
        for (auto mip = 1u; mip < mip_levels - 1; mip++)
            AddDownsamplePass(inData.mOutputTexture, inData.mOutputTexture, mip, mip + 1);

        // upsample along the bloom chain
        for (auto mip = mip_levels - 1; mip > 0; mip--)
            AddUpsamplePass(inData.mOutputTexture, inData.mOutputTexture, mip, mip - 1);
    },

    [&inDevice](BloomData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        // clear or discard maybe?
    });
}



const ComposeData& AddComposePass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inBloomTexture, RenderGraphResourceID inInputTexture)
{
    return inRenderGraph.AddGraphicsPass<ComposeData>("COMPOSE PASS",
    [&](RenderGraphBuilder& inBuilder, IRenderPass* inRenderPass, ComposeData& inData)
    {
        inData.mOutputTexture = inBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = inRenderGraph.GetViewport().GetDisplaySize().x,
            .height = inRenderGraph.GetViewport().GetDisplaySize().y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = "COMPOSE OUTPUT"
        });

        inBuilder.RenderTarget(inData.mOutputTexture);

        inData.mInputTextureSRV = inBuilder.Read(inInputTexture);
        inData.mBloomTextureSRV = inBuilder.Read(inBloomTexture);

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

        const auto vp = inRenderGraph.GetViewport();

        const auto scissor = CD3DX12_RECT(0, 0, vp.GetDisplaySize().x, vp.GetDisplaySize().y);
        const auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(vp.GetDisplaySize().x), float(vp.GetDisplaySize().y));

        inCmdList->RSSetViewports(1, &viewport);
        inCmdList->RSSetScissorRects(1, &scissor);

        inCmdList.PushGraphicsConstants(ComposeRootConstants {
            .mBloomTexture = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mBloomTextureSRV)),
            .mInputTexture = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mInputTextureSRV)),
            .mExposure = inData.mExposure,
            .mBloomBlendFactor = inData.mBloomBlendFactor,
            .mChromaticAberrationStrength = inData.mChromaticAberrationStrength,
         });

        inCmdList->DrawInstanced(6, 1, 0, 0);
    });
}



const PreImGuiData& AddPreImGuiPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID ioDisplayTexture)
{
    return inRenderGraph.AddGraphicsPass<PreImGuiData>("PRE-UI PASS",

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
            .debugName = "IMGUI_INDEX_BUFFER"
        });

        inData.mVertexBuffer = ioRGBuilder.Create(Buffer::Desc
        {
            .size = max_buffer_size,
            .stride = sizeof(ImDrawVert),
            .usage = Buffer::Usage::VERTEX_BUFFER,
            .debugName = "IMGUI_VERTEX_BUFFER"
        });

        inData.mInputTextureSRV = ioRGBuilder.Read(inInputTexture);
        inData.mBackBufferRTV = ioRGBuilder.RenderTarget(ioRGBuilder.Import(inDevice, inBackBuffer));

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
            inStagingHeap.StageBuffer(inCmdList, inDevice.GetBuffer(inData.mVertexBuffer), 0, inData.mVertexScratchBuffer.data(), vertex_buffer_size);

        if (index_buffer_size)
            inStagingHeap.StageBuffer(inCmdList, inDevice.GetBuffer(inData.mIndexBuffer), 0, inData.mIndexScratchBuffer.data(), index_buffer_size);

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




} // raekor
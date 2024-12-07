#include "PCH.h"
#include "RenderPasses.h"
#include "Shader.h"

#include "Timer.h"
#include "Camera.h"
#include "Components.h"
#include "Primitives.h"
#include "DebugRenderer.h"

namespace RK::DX12 {

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

const DefaultTexturesData& AddDefaultTexturesPass(RenderGraph& inRenderGraph, Device& inDevice, TextureID inBlackTexture, TextureID inWhiteTexture)
{
    return inRenderGraph.AddGraphicsPass<DefaultTexturesData>("DEFAULT TEXTURES PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, DefaultTexturesData& inData)
    {
        inData.mBlackTexture = ioRGBuilder.Import(inDevice, inBlackTexture);
        inData.mWhiteTexture = ioRGBuilder.Import(inDevice, inWhiteTexture);
    },
    [](DefaultTexturesData& inData, const RenderGraphResources& inResources, CommandList& inCmdList) {});
}



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
            .mBuffer = inResources.GetBindlessHeapIndex(inData.mBufferUAV)
        });

        const Buffer& buffer = inDevice.GetBuffer(inResources.GetBufferView(inData.mBufferUAV));

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
            .mTexture = inResources.GetBindlessHeapIndex(inData.mTextureUAV)
        });

        const Texture& texture = inDevice.GetTexture(inResources.GetTextureView(inData.mTextureUAV));

        inCmdList->SetPipelineState(g_SystemShaders.mClearTextureShader.GetComputePSO());
        inCmdList->Dispatch(( texture.GetWidth() + 7 ) / 8, ( texture.GetHeight() + 7 ) / 8, 1);
    });
}



const TransitionResourceData& AddTransitionResourcePass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphBuilderFunction inFunction, RenderGraphResourceID inResource)
{
    return inRenderGraph.AddGraphicsPass<TransitionResourceData>(std::format("EXPLICIT RESOURCE TRANSITION"),
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, TransitionResourceData& inData) { inData.mResourceView = (ioRGBuilder.*inFunction)(inResource); },
    [&inDevice](TransitionResourceData& inData, const RenderGraphResources& inResources, CommandList& inCmdList) {});
}



const SkyCubeData& AddSkyCubePass(RenderGraph& inRenderGraph, Device& inDevice, const Scene& inScene)
{
    return inRenderGraph.AddComputePass<SkyCubeData>("SKY CUBE PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, SkyCubeData& inData)
    {  
        inData.mSkyCubeTexture = ioRGBuilder.Create(Texture::DescCube(DXGI_FORMAT_R32G32B32A32_FLOAT, 64, 64, Texture::SHADER_READ_WRITE));
    },
    [&inDevice, &inScene](SkyCubeData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {   
        if (const DirectionalLight* sun_light = inScene.GetSunLight())
        {
            inCmdList.PushComputeConstants(SkyCubeRootConstants
            {
                .mSkyCubeTexture    = inResources.GetBindlessHeapIndex(inData.mSkyCubeTexture),
                .mSunLightDirection = sun_light->GetDirection(),
                .mSunLightColor     = sun_light->GetColor()
            });

            inCmdList->SetPipelineState(g_SystemShaders.mSkyCubeShader.GetComputePSO());

            const Texture::Desc& texture_desc = inDevice.GetTexture(inResources.GetTexture(inData.mSkyCubeTexture)).GetDesc();

            inCmdList->Dispatch(texture_desc.width / 8, texture_desc.height / 8, texture_desc.depthOrArrayLayers);
        }
    });
}



const ConvolveCubeData& AddConvolveSkyCubePass(RenderGraph& inRenderGraph, Device& inDevice, const SkyCubeData& inSkyCubeData)
{
    return inRenderGraph.AddGraphicsPass<ConvolveCubeData>("CONVOLVE SKYCUBE PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ConvolveCubeData& inData)
    {
        inData.mCubeTextureSRV = ioRGBuilder.Read(inSkyCubeData.mSkyCubeTexture);
        inData.mConvolvedCubeTexture = ioRGBuilder.Create(Texture::DescCube(DXGI_FORMAT_R32G32B32A32_FLOAT, 16, 16, Texture::SHADER_READ_WRITE));
    },

    [&inDevice](ConvolveCubeData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        inCmdList.PushComputeConstants(ConvolveCubeRootConstants
        {
            .mCubeTexture = inResources.GetBindlessHeapIndex(inData.mCubeTextureSRV),
            .mConvolvedCubeTexture = inResources.GetBindlessHeapIndex(inData.mConvolvedCubeTexture)
        });

        inCmdList->SetPipelineState(g_SystemShaders.mConvolveCubeShader.GetComputePSO());

        const Texture::Desc& texture_desc = inDevice.GetTexture(inResources.GetTexture(inData.mConvolvedCubeTexture)).GetDesc();

        inCmdList->Dispatch(texture_desc.width / 8, texture_desc.height / 8, texture_desc.depthOrArrayLayers);
    });
}



const SkinningData& AddSkinningPass(RenderGraph& inRenderGraph, Device& inDevice, const Scene& inScene)
{
    return inRenderGraph.AddComputePass<SkinningData>("SKINNING PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, SkinningData& inData) 
    {
    },
    [&inDevice, &inScene](SkinningData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        inCmdList->SetPipelineState(g_SystemShaders.mSkinningShader.GetComputePSO());

        Array<D3D12_RESOURCE_BARRIER> uav_barriers;
        uav_barriers.reserve(inScene.Count<Skeleton>());
        
        for (const auto& [entity, mesh, skeleton] : inScene.Each<Mesh, Skeleton>())
        {
            if (const Name* name = inScene.GetPtr<Name>(entity))
                PIXScopedEvent(static_cast<ID3D12GraphicsCommandList*>(inCmdList), PIX_COLOR(0, 255, 0), name->name.c_str());

            const Buffer& bone_matrix_buffer = inDevice.GetBuffer(TextureID(skeleton.boneTransformsBuffer));
        
            auto uav_to_copy_barrier = CD3DX12_RESOURCE_BARRIER::Transition(bone_matrix_buffer.GetD3D12Resource().Get(), GetD3D12ResourceStates(Texture::SHADER_READ_ONLY), D3D12_RESOURCE_STATE_COPY_DEST);
            inCmdList->ResourceBarrier(1, &uav_to_copy_barrier);

            const Mat4x4* bone_matrices_data = skeleton.boneTransformMatrices.data();
            const size_t bone_matrices_size = skeleton.boneTransformMatrices.size() * sizeof(Mat4x4);

            inDevice.UploadBufferData(inCmdList, bone_matrix_buffer, 0, bone_matrices_data, bone_matrices_size);

            auto copy_to_uav_barrier = CD3DX12_RESOURCE_BARRIER::Transition(bone_matrix_buffer.GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_COPY_DEST, GetD3D12ResourceStates(Texture::SHADER_READ_ONLY));
            inCmdList->ResourceBarrier(1, &copy_to_uav_barrier);

            inCmdList.PushComputeConstants(SkinningRootConstants 
            {
                .mBoneIndicesBuffer     = inDevice.GetBindlessHeapIndex(BufferID(skeleton.boneIndexBuffer)),
                .mBoneWeightsBuffer     = inDevice.GetBindlessHeapIndex(BufferID(skeleton.boneWeightBuffer)),
                .mMeshVertexBuffer      = inDevice.GetBindlessHeapIndex(BufferID(mesh.vertexBuffer)),
                .mSkinnedVertexBuffer   = inDevice.GetBindlessHeapIndex(BufferID(skeleton.skinnedVertexBuffer)),
                .mBoneTransformsBuffer  = inDevice.GetBindlessHeapIndex(BufferID(skeleton.boneTransformsBuffer)),
                .mDispatchSize          = uint32_t(mesh.positions.size())
            });

            inCmdList->Dispatch((mesh.positions.size() + 63) / 64, 1, 1);

            const Buffer& skinned_vertex_buffer = inDevice.GetBuffer(TextureID(skeleton.skinnedVertexBuffer));
            uav_barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(skinned_vertex_buffer.GetD3D12Resource().Get()));
        }

        if (uav_barriers.size())
            inCmdList->ResourceBarrier(uav_barriers.size(), uav_barriers.data());
    });
}


const GBufferData& AddMeshletsRasterPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene)
{
    return inRenderGraph.AddGraphicsPass<GBufferData>("MESHLETS RASTER PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, GBufferData& inData)
    {
        inData.mOutput.mRenderTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = "RT_GBufferRender"
        });

        inData.mOutput.mVelocityTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = "RT_GBufferVelocity"
        });

        inData.mOutput.mDepthTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::DEPTH_STENCIL_TARGET,
            .debugName = "RT_GBufferDepth"
        });

        ioRGBuilder.RenderTarget(inData.mOutput.mRenderTexture); // SV_Target0
        ioRGBuilder.RenderTarget(inData.mOutput.mVelocityTexture); // SV_Target1
        ioRGBuilder.DepthStencilTarget(inData.mOutput.mDepthTexture);

        ByteSlice vertex_shader, pixel_shader;
        g_SystemShaders.mGBufferShader.GetGraphicsProgram(vertex_shader, pixel_shader);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inRenderPass->CreatePipelineStateDesc(inDevice, vertex_shader, pixel_shader);
        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(inData.mPipeline.GetAddressOf()));
        inData.mPipeline->SetName(L"PSO_MESHLETS_RASTER");
    },

    [&inRenderGraph, &inDevice, &inScene](GBufferData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        const Viewport& viewport = inRenderGraph.GetViewport();
        inCmdList.SetViewportAndScissor(viewport);
        inCmdList->SetPipelineState(inData.mPipeline.Get());

        constexpr Vec4 clear_color = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        inCmdList->ClearDepthStencilView(inDevice.GetCPUDescriptorHandle(inResources.GetTexture(inData.mOutput.mDepthTexture)), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
        inCmdList->ClearRenderTargetView(inDevice.GetCPUDescriptorHandle(inResources.GetTexture(inData.mOutput.mRenderTexture)), glm::value_ptr(clear_color), 0, nullptr);
        inCmdList->ClearRenderTargetView(inDevice.GetCPUDescriptorHandle(inResources.GetTexture(inData.mOutput.mVelocityTexture)), glm::value_ptr(clear_color), 0, nullptr);

        for (const auto& [entity, mesh] : inScene->Each<Mesh>())
        {
            const Transform* transform = inScene->GetPtr<Transform>(entity);
            if (!transform)
                continue;

            if (!BufferID(mesh.BottomLevelAS).IsValid())
                continue;

            if (mesh.meshlets.empty())
                continue;

            const Material* material = inScene->GetPtr<Material>(mesh.material);
            //if (material && material->isTransparent)
                //continue;

            const Buffer& index_buffer  = inDevice.GetBuffer(BufferID(mesh.indexBuffer));
            const Buffer& vertex_buffer = inDevice.GetBuffer(BufferID(mesh.vertexBuffer));

            const D3D12_INDEX_BUFFER_VIEW index_view =
            {
                .BufferLocation = index_buffer->GetGPUVirtualAddress(),
                .SizeInBytes = uint32_t(mesh.indices.size() * sizeof(mesh.indices[0])),
                .Format = DXGI_FORMAT_R32_UINT,
            };

            if (mesh.material == Entity::Null)
                continue;

            if (material == nullptr)
                material = &Material::Default;

            int material_index = inScene->GetPackedIndex<Material>(mesh.material);
            material_index = material_index == -1 ? 0 : material_index;

            const int instance_index = inScene->GetPackedIndex<Mesh>(entity);
            assert(instance_index != -1);

            if (const Name* name = inScene->GetPtr<Name>(entity))
                PIXScopedEvent(static_cast<ID3D12GraphicsCommandList*>( inCmdList ), PIX_COLOR(0, 255, 0), name->name.c_str());

            const GbufferRootConstants root_constants = 
            {
                .mInstancesBuffer = inScene.GetInstancesDescriptorIndex(),
                .mMaterialsBuffer = inScene.GetMaterialsDescriptorIndex(),
                .mInstanceIndex   = uint32_t(instance_index)
            };

            inCmdList->SetGraphicsRoot32BitConstants(0, sizeof(root_constants) / sizeof(DWORD), &root_constants, 0);

            inCmdList->IASetIndexBuffer(&index_view);

            if (entity == RenderSettings::mActiveEntity)
            {
                // do stencil stuff?
            }

            inCmdList->DrawIndexedInstanced(mesh.indices.size(), 1, 0, 0, 0);
        }
    });
}



const GBufferData& AddGBufferPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene)
{
    return inRenderGraph.AddGraphicsPass<GBufferData>("GBUFFER PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, GBufferData& inData)
    {
        inData.mOutput.mRenderTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = "RT_GBufferColor"
        });

        inData.mOutput.mSelectionTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32_UINT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = "RT_GBufferSelection"
        });

        inData.mOutput.mVelocityTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = "RT_GBufferVelocity"
        });

        inData.mOutput.mDepthTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::DEPTH_STENCIL_TARGET,
            .debugName = "RT_GBufferDepth"
        });

        ioRGBuilder.RenderTarget(inData.mOutput.mRenderTexture); // SV_Target0
        ioRGBuilder.RenderTarget(inData.mOutput.mVelocityTexture); // SV_Target1
        ioRGBuilder.RenderTarget(inData.mOutput.mSelectionTexture); // SV_Target2
        ioRGBuilder.DepthStencilTarget(inData.mOutput.mDepthTexture);

        ByteSlice vertex_shader, pixel_shader;
        g_SystemShaders.mGBufferShader.GetGraphicsProgram(vertex_shader, pixel_shader);

        const ShaderProgram& shader_program = g_SystemShaders.mGBufferShader;


        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inRenderPass->CreatePipelineStateDesc(inDevice, vertex_shader, pixel_shader);
        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(inData.mPipeline.GetAddressOf()));
        inData.mPipeline->SetName(L"PSO_GBUFFER");

        // store the render pass ptr so we can access it during execution
        inData.mRenderPass = inRenderPass;
    },

    [&inDevice, &inScene](GBufferData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.SetViewportAndScissor(inDevice.GetTexture(inResources.GetTexture(inData.mOutput.mRenderTexture)));

        constexpr Vec4 clear_color = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        inCmdList->ClearDepthStencilView(inDevice.GetCPUDescriptorHandle(inResources.GetTexture(inData.mOutput.mDepthTexture)), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
        inCmdList->ClearRenderTargetView(inDevice.GetCPUDescriptorHandle(inResources.GetTexture(inData.mOutput.mRenderTexture)), glm::value_ptr(clear_color), 0, nullptr);
        inCmdList->ClearRenderTargetView(inDevice.GetCPUDescriptorHandle(inResources.GetTexture(inData.mOutput.mVelocityTexture)), glm::value_ptr(clear_color), 0, nullptr);
        inCmdList->ClearRenderTargetView(inDevice.GetCPUDescriptorHandle(inResources.GetTexture(inData.mOutput.mSelectionTexture)), glm::value_ptr(clear_color), 0, nullptr);

        Timer timer;

        for (const auto& [entity, mesh] : inScene->Each<Mesh>())
        {
            const Name& name = inScene->Get<Name>(entity);
            const Transform* transform = inScene->GetPtr<Transform>(entity);

            if (!transform)
                continue;

            if (!BufferID(mesh.BottomLevelAS).IsValid())
                continue;

            if (!mesh.meshlets.empty())
                continue;

            //if (material && material->isTransparent)
                //continue;

            const Material* material = inScene->GetPtr<Material>(mesh.material);

            if (material == nullptr)
                material = &Material::Default;

            uint64_t pixel_shader = material->pixelShader ? material->pixelShader : g_SystemShaders.mGBufferShader.GetPixelShaderHash();
            uint64_t vertex_shader = material->vertexShader ? material->vertexShader : g_SystemShaders.mGBufferShader.GetVertexShaderHash();

            if (ID3D12PipelineState* pipeline_state = g_ShaderCompiler.GetGraphicsPipeline(inDevice, inData.mRenderPass, vertex_shader, pixel_shader))
                inCmdList->SetPipelineState(pipeline_state);
            else
                continue;

            const char* debug_name = mesh.name.empty() ? name.name.c_str() : mesh.name.c_str();
            PIXScopedEvent(static_cast<ID3D12GraphicsCommandList*>( inCmdList ), PIX_COLOR(0, 255, 0), debug_name);

            const int instance_index = inScene->GetPackedIndex<Mesh>(entity);
            assert(instance_index != -1);

            inCmdList.PushGraphicsConstants(GbufferRootConstants
            {
                .mInstancesBuffer = inScene.GetInstancesDescriptorIndex(),
                .mMaterialsBuffer = inScene.GetMaterialsDescriptorIndex(),
                .mInstanceIndex = uint32_t(instance_index),
                .mEntity = uint32_t(entity)
            });

            inCmdList.BindIndexBuffer(inDevice.GetBuffer(BufferID(mesh.indexBuffer)));

            if (entity == RenderSettings::mActiveEntity)
            {
                // do stencil stuff?
            }

            inCmdList->DrawIndexedInstanced(mesh.indices.size(), 1, 0, 0, 0);
        }
    });
}



const GBufferDebugData& AddGBufferDebugPass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferOutput& inGBuffer, EDebugTexture inDebugTexture)
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
            .debugName = "RT_GBufferDebug"
        });

        ioRGBuilder.RenderTarget(inData.mOutputTexture);

        switch (inDebugTexture)
        {
            case DEBUG_TEXTURE_GBUFFER_DEPTH:     inData.mInputTextureSRV = ioRGBuilder.Read(inGBuffer.mDepthTexture);    break;
            case DEBUG_TEXTURE_GBUFFER_ALBEDO:    inData.mInputTextureSRV = ioRGBuilder.Read(inGBuffer.mRenderTexture);   break;
            case DEBUG_TEXTURE_GBUFFER_NORMALS:   inData.mInputTextureSRV = ioRGBuilder.Read(inGBuffer.mRenderTexture);   break;
            case DEBUG_TEXTURE_GBUFFER_EMISSIVE:  inData.mInputTextureSRV = ioRGBuilder.Read(inGBuffer.mRenderTexture);   break;
            case DEBUG_TEXTURE_GBUFFER_VELOCITY:  inData.mInputTextureSRV = ioRGBuilder.Read(inGBuffer.mVelocityTexture); break;
            case DEBUG_TEXTURE_GBUFFER_METALLIC:  inData.mInputTextureSRV = ioRGBuilder.Read(inGBuffer.mRenderTexture);   break;
            case DEBUG_TEXTURE_GBUFFER_ROUGHNESS: inData.mInputTextureSRV = ioRGBuilder.Read(inGBuffer.mRenderTexture);   break;
            default: assert(false);
        }

        ByteSlice vertex_shader, pixel_shader;
        switch (inDebugTexture)
        {
            case DEBUG_TEXTURE_GBUFFER_DEPTH:     g_SystemShaders.mGBufferDebugDepthShader.GetGraphicsProgram(vertex_shader, pixel_shader);      break;
            case DEBUG_TEXTURE_GBUFFER_ALBEDO:    g_SystemShaders.mGBufferDebugAlbedoShader.GetGraphicsProgram(vertex_shader, pixel_shader);     break;
            case DEBUG_TEXTURE_GBUFFER_NORMALS:   g_SystemShaders.mGBufferDebugNormalsShader.GetGraphicsProgram(vertex_shader, pixel_shader);    break;
            case DEBUG_TEXTURE_GBUFFER_EMISSIVE:  g_SystemShaders.mGBufferDebugEmissiveShader.GetGraphicsProgram(vertex_shader, pixel_shader);   break;
            case DEBUG_TEXTURE_GBUFFER_VELOCITY:  g_SystemShaders.mGBufferDebugVelocityShader.GetGraphicsProgram(vertex_shader, pixel_shader);   break;
            case DEBUG_TEXTURE_GBUFFER_METALLIC:  g_SystemShaders.mGBufferDebugMetallicShader.GetGraphicsProgram(vertex_shader, pixel_shader);   break;
            case DEBUG_TEXTURE_GBUFFER_ROUGHNESS: g_SystemShaders.mGBufferDebugRoughnessShader.GetGraphicsProgram(vertex_shader, pixel_shader);  break;
            default: assert(false);
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC state = inRenderPass->CreatePipelineStateDesc(inDevice, vertex_shader, pixel_shader);
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
            .mTexture   = inRGResources.GetBindlessHeapIndex(inData.mInputTextureSRV),
            .mFarPlane  = inRenderGraph.GetViewport().GetFar(),
            .mNearPlane = inRenderGraph.GetViewport().GetNear(),
        });
        inCmdList->DrawInstanced(3, 1, 0, 0);
    });
}



const ShadowMapData& AddShadowMapPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene)
{
    return inRenderGraph.AddGraphicsPass<ShadowMapData>("SHADOW MAP PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ShadowMapData& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc {
            .format = DXGI_FORMAT_D32_FLOAT,
            .width  = 2048,
            .height = 2048,
            .usage  = Texture::Usage::DEPTH_STENCIL_TARGET,
            .debugName = "ShadowMap2048"
        });

        ioRGBuilder.DepthStencilTarget(inData.mOutputTexture);

        ByteSlice vertex_shader, pixel_shader;
        g_SystemShaders.mShadowMapShader.GetGraphicsProgram(vertex_shader, pixel_shader);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = inRenderPass->CreatePipelineStateDesc(inDevice, vertex_shader, pixel_shader);

        inDevice->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(inData.mPipeline.GetAddressOf()));
    },

    [&inDevice, &inRenderGraph, &inScene](ShadowMapData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        if (inScene->Count<Mesh>() == 0)
            return;

        std::array frustum_corners = {
            Vec3(-1.0f,  1.0f, -1.0f),
            Vec3( 1.0f,  1.0f, -1.0f),
            Vec3( 1.0f, -1.0f, -1.0f),
            Vec3(-1.0f, -1.0f, -1.0f),
            Vec3(-1.0f,  1.0f,  1.0f),
            Vec3( 1.0f,  1.0f,  1.0f),
            Vec3( 1.0f, -1.0f,  1.0f),
            Vec3(-1.0f, -1.0f,  1.0f),
        };

        Mat4x4 projection = glm::perspectiveRH(
            glm::radians(inRenderGraph.GetViewport().GetFieldOfView()),
            inRenderGraph.GetViewport().GetAspectRatio(),
            inRenderGraph.GetViewport().GetNear(),
            128.0f
        );

        const Viewport& vp = inRenderGraph.GetViewport();
        const Mat4x4 inv_vp = glm::inverse(projection * vp.GetView());

        for (Vec3& corner : frustum_corners)
        {
            Vec4 corner_ws = inv_vp * Vec4(corner, 1.0f);
            corner = corner_ws / corner_ws.w;
        }

        float radius = 0.0f;
        Vec3 frustum_center = Vec3(0.0f);

        for (const Vec3& corner : frustum_corners)
            frustum_center += corner;
        
        frustum_center /= 8.0f;

        for (const Vec3& corner : frustum_corners)
            radius = glm::max(radius, glm::length(corner - frustum_center));

        radius = std::ceil(radius * 16.0f) / 16.0f;

        const Vec3 max_extents = Vec3(radius);
        const Vec3 min_extents = -max_extents;

        const Vec3 light_dir = glm::normalize(inScene->GetSunLightDirection());

        const Mat4x4 light_view_matrix = glm::lookAtRH(frustum_center + light_dir * min_extents.z, frustum_center, Vec3(0.0f, 1.0f, 0.0f));
        const Mat4x4 light_ortho_matrix = glm::orthoRH_ZO(min_extents.x, max_extents.x, min_extents.y, max_extents.y, 0.0f, max_extents.z - min_extents.z);

        inCmdList->SetPipelineState(inData.mPipeline.Get());

        TextureID texture = inResources.GetTexture(inData.mOutputTexture);

        D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = inDevice.GetCPUDescriptorHandle(texture);
        inCmdList->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        inCmdList.SetViewportAndScissor(inDevice.GetTexture(texture));

        if (inScene->Count<Mesh>() > 0)
        {
            ShadowMapRootConstants root_constants =
            {
                .mInstancesBuffer = inScene.GetTLASDescriptorIndex(),
                .mMaterialsBuffer = inScene.GetMaterialsDescriptorIndex(),
                .mViewProjMatrix = light_ortho_matrix * light_view_matrix
            };

            for (const auto& [entity, mesh] : inScene->Each<Mesh>())
            {
                const Name& name = inScene->Get<Name>(entity);

                const char* debug_name = mesh.name.empty() ? name.name.c_str() : mesh.name.c_str();
                PIXScopedEvent(static_cast<ID3D12GraphicsCommandList*>( inCmdList ), PIX_COLOR(0, 255, 0), debug_name);

                const int instance_index = inScene->GetPackedIndex<Mesh>(entity);
                assert(instance_index != -1);

                root_constants.mInstanceIndex = uint32_t(instance_index);
                inCmdList.PushGraphicsConstants(root_constants);

                inCmdList.BindIndexBuffer(inDevice.GetBuffer(BufferID(mesh.indexBuffer)));
                inCmdList->DrawIndexedInstanced(mesh.indices.size(), 1, 0, 0, 0);
            }
        }

    });
}



const SSAOTraceData& AddSSAOTracePass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferOutput& inGBuffer)
{
    return inRenderGraph.AddComputePass<SSAOTraceData>("SSAO TRACE PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, SSAOTraceData& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc 
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage  = Texture::Usage::SHADER_READ_WRITE
        });

        inData.mDepthTexture   = ioRGBuilder.Read(inGBuffer.mDepthTexture);
        inData.mGBufferTexture = ioRGBuilder.Read(inGBuffer.mRenderTexture);
    },

    [&inRenderGraph, &inDevice](SSAOTraceData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        const Viewport& viewport = inRenderGraph.GetViewport();

        inCmdList.PushComputeConstants(SSAOTraceRootConstants 
        {
            .mOutputTexture  = inRGResources.GetBindlessHeapIndex(inData.mOutputTexture),
            .mDepthTexture   = inRGResources.GetBindlessHeapIndex(inData.mDepthTexture),
            .mGBufferTexture = inRGResources.GetBindlessHeapIndex(inData.mGBufferTexture),
            .mRadius         = RenderSettings::mSSAORadius,
            .mBias           = RenderSettings::mSSAOBias,
            .mSamples        = uint32_t(RenderSettings::mSSAOSamples),
            .mDispatchSize   = viewport.size
        });

        inCmdList->SetPipelineState(g_SystemShaders.mSSAOTraceShader.GetComputePSO());
        inCmdList->Dispatch(( viewport.size.x + 7 ) / 8, ( viewport.size.y + 7 ) / 8, 1);
    });
}



const SSRTraceData& AddSSRTracePass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferOutput& inGBuffer, RenderGraphResourceID inSceneTexture)
{
    return inRenderGraph.AddComputePass<SSRTraceData>("SSR TRACE PASS",
        [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, SSRTraceData& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
            {
                .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
                .width  = inRenderGraph.GetViewport().GetRenderSize().x,
                .height = inRenderGraph.GetViewport().GetRenderSize().y,
                .usage  = Texture::Usage::SHADER_READ_WRITE
            });

        inData.mSceneTexture = ioRGBuilder.Read(inSceneTexture);
        inData.mDepthTexture = ioRGBuilder.Read(inGBuffer.mDepthTexture);
        inData.mGBufferTexture = ioRGBuilder.Read(inGBuffer.mRenderTexture);
    },

        [&inRenderGraph, &inDevice](SSRTraceData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        const Viewport& viewport = inRenderGraph.GetViewport();

        inCmdList.PushComputeConstants(SSRTraceRootConstants
            {
                .mOutputTexture = inRGResources.GetBindlessHeapIndex(inData.mOutputTexture),
                .mSceneTexture = inRGResources.GetBindlessHeapIndex(inData.mSceneTexture),
                .mDepthTexture = inRGResources.GetBindlessHeapIndex(inData.mDepthTexture),
                .mGBufferTexture = inRGResources.GetBindlessHeapIndex(inData.mGBufferTexture),
                .mRadius = RenderSettings::mSSRRadius,
                .mBias = RenderSettings::mSSRBias,
                .mSamples = uint32_t(RenderSettings::mSSRSamples),
                .mDispatchSize = viewport.size
            });

        inCmdList->SetPipelineState(g_SystemShaders.mSSRTraceShader.GetComputePSO());
        inCmdList->Dispatch(( viewport.size.x + 7 ) / 8, ( viewport.size.y + 7 ) / 8, 1);
    });
}



const GrassData& AddGrassRenderPass(RenderGraph& inGraph, Device& inDevice, const GBufferOutput& inGBuffer)
{
    return inGraph.AddGraphicsPass<GrassData>("GRASS DRAW PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, GrassData& inData)
    {
        inData.mRenderTextureSRV = ioRGBuilder.RenderTarget(inGBuffer.mRenderTexture);
        inData.mDepthTextureSRV  = ioRGBuilder.DepthStencilTarget(inGBuffer.mDepthTexture);

        ByteSlice vertex_shader, pixel_shader;
        g_SystemShaders.mGrassShader.GetGraphicsProgram(vertex_shader, pixel_shader);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = inRenderPass->CreatePipelineStateDesc(inDevice, vertex_shader, pixel_shader);
        pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inDevice->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&inData.mPipeline));
        inData.mPipeline->SetName(L"PSO_GRASS_DRAW");
    },

    [](GrassData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        const int blade_vertex_count = 15;

        inCmdList.PushGraphicsConstants(GrassRenderRootConstants {
            .mBend = RenderSettings::mGrassBend,
            .mTilt = RenderSettings::mGrassTilt,
            .mWindDirection = RenderSettings::mWindDirection
        });

        inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->DrawInstanced(blade_vertex_count, 65536, 0, 0);
        inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    });
}



const DownsampleData& AddDownsamplePass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inSourceTexture)
{
    return inRenderGraph.AddComputePass<DownsampleData>("DOWNSAMPLE PASS",
    [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, DownsampleData& inData)
    {
        inData.mGlobalAtomicBuffer = inRGBuilder.Create(Buffer::RWStructuredBuffer(sizeof(uint32_t), sizeof(uint32_t), "AtomicUintBuffer"));

        inData.mSourceTextureUAV = inRGBuilder.Write(inSourceTexture);
        /* inData.mGlobalAtomicBuffer */ inRGBuilder.Write(inData.mGlobalAtomicBuffer);

        const RenderGraphResourceDesc& texture_desc = inRGBuilder.GetResourceDesc(inSourceTexture);
        assert(texture_desc.mResourceType == RESOURCE_TYPE_TEXTURE);

        const uint32_t nr_of_mips = gSpdCaculateMipCount(texture_desc.mTextureDesc.width, texture_desc.mTextureDesc.height);

        assert(false); // TODO: RenderGraphBuilder::Write with subresource specifier
        for (int mip = 0u; mip < nr_of_mips; mip++)
        {
            // inRGBuilder.Write(inSourceTexture, mip);
        }

        ByteSlice compute_shader;
        g_SystemShaders.mDownsampleShader.GetComputeProgram(compute_shader);
        D3D12_COMPUTE_PIPELINE_STATE_DESC state = inRenderPass->CreatePipelineStateDesc(inDevice, compute_shader);
    },

    [&inRenderGraph, &inDevice](DownsampleData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        const Texture& texture = inDevice.GetTexture(inRGResources.GetTextureView(inData.mSourceTextureUAV));
        const UVec4 rect_info = UVec4(0u, 0u, texture->GetDesc().Width, texture->GetDesc().Height);

        glm::uvec2 work_group_offset, dispatchThreadGroupCountXY, numWorkGroupsAndMips;
        work_group_offset[0] = rect_info[0] / 64; // rectInfo[0] = left
        work_group_offset[1] = rect_info[1] / 64; // rectInfo[1] = top

        uint32_t endIndexX = ( rect_info[0] + rect_info[2] - 1 ) / 64; // rectInfo[0] = left, rectInfo[2] = width
        uint32_t endIndexY = ( rect_info[1] + rect_info[3] - 1 ) / 64; // rectInfo[1] = top, rectInfo[3] = height

        dispatchThreadGroupCountXY[0] = endIndexX + 1 - work_group_offset[0];
        dispatchThreadGroupCountXY[1] = endIndexY + 1 - work_group_offset[1];

        numWorkGroupsAndMips[0] = ( dispatchThreadGroupCountXY[0] ) * ( dispatchThreadGroupCountXY[1] );
        numWorkGroupsAndMips[1] = gSpdCaculateMipCount(rect_info[2], rect_info[3]);

        const Buffer& atomic_buffer = inDevice.GetBuffer(inData.mGlobalAtomicBuffer);

        SpdRootConstants root_constants =
        {
            .mNrOfMips = numWorkGroupsAndMips[1],
            .mNrOfWorkGroups = numWorkGroupsAndMips[0],
            .mGlobalAtomicBuffer = inRGResources.GetBindlessHeapIndex(inData.mGlobalAtomicBuffer),
            .mWorkGroupOffset = work_group_offset,
        };

        uint* mips_ptr = &root_constants.mTextureMip0;

        for (uint32_t mip = 0u; mip < numWorkGroupsAndMips[1]; mip++)
            mips_ptr[mip] = inRGResources.GetBindlessHeapIndex(inData.mSourceTextureMipsUAVs[mip]);

        static bool first_run = true;

        if (first_run)
        {
            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(atomic_buffer.GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);

            inCmdList->ResourceBarrier(1, &barrier);

            const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER param = { .Dest = atomic_buffer->GetGPUVirtualAddress(), .Value = 0 };
            inCmdList->WriteBufferImmediate(1, &param, nullptr);

            barrier = CD3DX12_RESOURCE_BARRIER::Transition(atomic_buffer.GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            inCmdList->ResourceBarrier(1, &barrier);

            first_run = false;
        }

        inCmdList->SetPipelineState(g_SystemShaders.mDownsampleShader.GetComputePSO());
        inCmdList.PushComputeConstants(root_constants);
        inCmdList->Dispatch(dispatchThreadGroupCountXY.x, dispatchThreadGroupCountXY.y, 1);
    });
}



const TiledLightCullingData& AddTiledLightCullingPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene)
{
    return inRenderGraph.AddComputePass<TiledLightCullingData>("TILED LIGHT CULLING PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, TiledLightCullingData& inData)
    {
        const UVec2 render_size = inRenderGraph.GetViewport().GetRenderSize();
        const UVec2 nr_of_tiles = UVec2
        (
            (render_size.x + LIGHT_CULL_TILE_SIZE - 1) / LIGHT_CULL_TILE_SIZE, 
            (render_size.y + LIGHT_CULL_TILE_SIZE - 1) / LIGHT_CULL_TILE_SIZE
        );

        inData.mLightGridBuffer = ioRGBuilder.Create(Buffer::RWByteAddressBuffer(nr_of_tiles.x * nr_of_tiles.y, "LightCullingLightGrid"));
        inData.mLightIndicesBuffer = ioRGBuilder.Create(Buffer::RWByteAddressBuffer(nr_of_tiles.x * nr_of_tiles.y * LIGHT_CULL_MAX_LIGHTS, "LightCullingIndices"));

        ioRGBuilder.Write(inData.mLightGridBuffer);
        ioRGBuilder.Write(inData.mLightIndicesBuffer);
    },
    [&inRenderGraph, &inScene](TiledLightCullingData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        inData.mRootConstants = 
        {
            .mLightGridBuffer    = inRGResources.GetBindlessHeapIndex(inData.mLightGridBuffer),
            .mLightIndicesBuffer = inRGResources.GetBindlessHeapIndex(inData.mLightIndicesBuffer),
        };

        //inData.mRootConstants.mFullResSize = inRenderGraph.GetViewport().GetRenderSize();
        //inData.mRootConstants.mDispatchSize.x = (inData.mRootConstants.mFullResSize.x + LIGHT_CULL_TILE_SIZE - 1) / LIGHT_CULL_TILE_SIZE;
        //inData.mRootConstants.mDispatchSize.y = (inData.mRootConstants.mFullResSize.y + LIGHT_CULL_TILE_SIZE - 1) / LIGHT_CULL_TILE_SIZE;
//
        //inData.mRootConstants.mLightsCount = inScene->Count<Light>();
        //if (inData.mRootConstants.mLightsCount > 0)
        //    inData.mRootConstants.mLightsBuffer = inDevice.GetBindlessHeapIndex(inScene.GetLightsDescriptor(inDevice));
//
        //inCmdList.PushComputeConstants(inData.mRootConstants);
        //inCmdList->SetPipelineState(g_SystemShaders.mLightCullShader.GetComputePSO());
//
        //inCmdList->Dispatch(inData.mRootConstants.mDispatchSize.x, inData.mRootConstants.mDispatchSize.y, 1);
    });
}



const LightingData& AddLightingPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferOutput& inGBuffer, const TiledLightCullingData& inLightCullData, RenderGraphResourceID inSkyCubeTexture, RenderGraphResourceID inShadowTexture, RenderGraphResourceID inReflectionsTexture, RenderGraphResourceID inAOTexture, RenderGraphResourceID inIndirectDiffuseTexture)
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
            .debugName = "RT_ShadingOutput"
        });

        ioRGBuilder.RenderTarget(inData.mOutputTexture);

        ioRGBuilder.Write(inLightCullData.mLightGridBuffer);
        ioRGBuilder.Write(inLightCullData.mLightIndicesBuffer);

        inData.mSkyCubeTextureSRV           = ioRGBuilder.Read(inSkyCubeTexture);
        inData.mAmbientOcclusionTextureSRV  = ioRGBuilder.Read(inAOTexture);
        inData.mShadowMaskTextureSRV        = ioRGBuilder.Read(inShadowTexture);
        inData.mReflectionsTextureSRV       = ioRGBuilder.Read(inReflectionsTexture);
        inData.mIndirectDiffuseTextureSRV   = ioRGBuilder.Read(inIndirectDiffuseTexture);

        inData.mGBufferDepthTextureSRV      = ioRGBuilder.Read(inGBuffer.mDepthTexture);
        inData.mGBufferRenderTextureSRV     = ioRGBuilder.Read(inGBuffer.mRenderTexture);

        ByteSlice vertex_shader, pixel_shader;
        g_SystemShaders.mLightingShader.GetGraphicsProgram(vertex_shader, pixel_shader);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inRenderPass->CreatePipelineStateDesc(inDevice, vertex_shader, pixel_shader);

        pso_state.InputLayout = {}; // clear the input layout, we generate the fullscreen triangle inside the vertex shader
        pso_state.DepthStencilState.DepthEnable = FALSE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(&inData.mPipeline));
        inData.mPipeline->SetName(L"PSO_DEFERRED_LIGHTING");
    },

    [&inRenderGraph, &inDevice, &inScene, &inLightCullData](LightingData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        LightingRootConstants root_constants =
        {
            .mSkyCubeTexture          = inRGResources.GetBindlessHeapIndex(inData.mSkyCubeTextureSRV),
            .mShadowMaskTexture       = inRGResources.GetBindlessHeapIndex(inData.mShadowMaskTextureSRV),
            .mReflectionsTexture      = inRGResources.GetBindlessHeapIndex(inData.mReflectionsTextureSRV),
            .mGbufferDepthTexture     = inRGResources.GetBindlessHeapIndex(inData.mGBufferDepthTextureSRV),
            .mGbufferRenderTexture    = inRGResources.GetBindlessHeapIndex(inData.mGBufferRenderTextureSRV),
            .mIndirectDiffuseTexture  = inRGResources.GetBindlessHeapIndex(inData.mIndirectDiffuseTextureSRV),
            .mAmbientOcclusionTexture = inRGResources.GetBindlessHeapIndex(inData.mAmbientOcclusionTextureSRV),
        };

        if (inScene.HasLights())
        {
            root_constants.mLightsCount = inScene.GetLightsCount();
            root_constants.mLightsBuffer = inScene.GetLightsDescriptorIndex();
        }

        root_constants.mLights = inLightCullData.mRootConstants;

        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.SetViewportAndScissor(inRenderGraph.GetViewport());
        inCmdList.PushGraphicsConstants(root_constants);
        inCmdList->DrawInstanced(3, 1, 0, 0);
    });
}



const TAAResolveData& AddTAAResolvePass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferOutput& inGBuffer, RenderGraphResourceID inColorTexture)
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
            .debugName = "RT_TAAOutput"
        });

        inData.mHistoryTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::SHADER_READ_ONLY,
            .debugName = "RT_TAAHistory"
        });

        ioRGBuilder.RenderTarget(inData.mOutputTexture);

        inData.mColorTextureSRV    =  ioRGBuilder.Read(inColorTexture);
        inData.mHistoryTextureSRV  =  ioRGBuilder.Read(inData.mHistoryTexture);
        inData.mDepthTextureSRV    =  ioRGBuilder.Read(inGBuffer.mDepthTexture);
        inData.mVelocityTextureSRV =  ioRGBuilder.Read(inGBuffer.mVelocityTexture);

        ByteSlice vertex_shader, pixel_shader;
        g_SystemShaders.mTAAResolveShader.GetGraphicsProgram(vertex_shader, pixel_shader);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inRenderPass->CreatePipelineStateDesc(inDevice, vertex_shader, pixel_shader);

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
            .mColorTexture    = inResources.GetBindlessHeapIndex(inData.mColorTextureSRV),
            .mDepthTexture    = inResources.GetBindlessHeapIndex(inData.mDepthTextureSRV),
            .mHistoryTexture  = inResources.GetBindlessHeapIndex(inData.mHistoryTextureSRV),
            .mVelocityTexture = inResources.GetBindlessHeapIndex(inData.mVelocityTextureSRV)
        });

        inCmdList->DrawInstanced(3, 1, 0, 0);

        ID3D12Resource* result_texture_resource = inDevice.GetD3D12Resource(inResources.GetTexture(inData.mOutputTexture));
        ID3D12Resource* history_texture_resource = inDevice.GetD3D12Resource(inResources.GetTexture(inData.mHistoryTexture));

        std::array barriers =
        {
            D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(result_texture_resource, GetD3D12ResourceStates(Texture::RENDER_TARGET), D3D12_RESOURCE_STATE_COPY_SOURCE)),
            D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(history_texture_resource, GetD3D12ResourceStates(Texture::SHADER_READ_ONLY), D3D12_RESOURCE_STATE_COPY_DEST))
        };
        inCmdList->ResourceBarrier(barriers.size(), barriers.data());

        const CD3DX12_TEXTURE_COPY_LOCATION dest = CD3DX12_TEXTURE_COPY_LOCATION(history_texture_resource, 0);
        const CD3DX12_TEXTURE_COPY_LOCATION source = CD3DX12_TEXTURE_COPY_LOCATION(result_texture_resource, 0);
        inCmdList->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);

        for (D3D12_RESOURCE_BARRIER& barrier : barriers)
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
            .debugName = "RT_DoFOutput"
        });

        inBuilder.Write(inData.mOutputTexture);
        
        inData.mDepthTextureSRV = inBuilder.Read(inDepthTexture);
        inData.mInputTextureSRV = inBuilder.Read(inInputTexture);
    },

    [&inRenderGraph, &inDevice](DepthOfFieldData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        const Viewport& viewport = inRenderGraph.GetViewport();

        inCmdList.PushComputeConstants(DepthOfFieldRootConstants {
            .mDepthTexture  = inRGResources.GetBindlessHeapIndex(inData.mDepthTextureSRV),
            .mInputTexture  = inRGResources.GetBindlessHeapIndex(inData.mInputTextureSRV),
            .mOutputTexture = inRGResources.GetBindlessHeapIndex(inData.mOutputTexture),
            .mFarPlane      = viewport.GetFar(),
            .mNearPlane     = viewport.GetNear(),
            .mFocusPoint    = RenderSettings::mDoFFocusPoint,
            .mFocusScale    = RenderSettings::mDoFFocusScale,
            .mDispatchSize  = viewport.size
        });

        inCmdList->SetPipelineState(g_SystemShaders.mDepthOfFieldShader.GetComputePSO());
        inCmdList->Dispatch((viewport.GetDisplaySize().x + 7) / 8, (viewport.GetDisplaySize().y + 7) / 8, 1);
    });
}



const LuminanceHistogramData& AddLuminanceHistogramPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inInputTexture) 
{
    return inRenderGraph.AddComputePass<LuminanceHistogramData>("LUMINANCE HISTOGRAM PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, LuminanceHistogramData& inData)
    {
        inData.mHistogramBuffer = ioRGBuilder.Create(Buffer::RWTypedBuffer(DXGI_FORMAT_R32_UINT, 128, "LumHistogramBuffer"));
        inData.mInputTextureSRV = ioRGBuilder.Write(inInputTexture);
    },

    [](LuminanceHistogramData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {

    });
}



const BloomPassData& AddBloomPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inInputTexture)
{
    auto RunBloomPass = [&](const String& inPassName, ID3D12PipelineState* inPipeline, RenderGraphResourceID inFromTexture, RenderGraphResourceID inToTexture, uint32_t inFromMip, uint32_t inToMip)
    {
        return inRenderGraph.AddComputePass<BloomBlurData>(inPassName,
        [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, BloomBlurData& inData)
        {
            inData.mToTextureMip   = inToMip;
            inData.mFromTextureMip = inFromMip;
            inData.mToTextureUAV   = ioRGBuilder.WriteTexture(inToTexture, inToMip);
            inData.mFromTextureSRV = ioRGBuilder.ReadTexture(inFromTexture, inFromMip);
        },
        [&inRenderGraph, &inDevice, inPipeline](BloomBlurData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
        {
            const CD3DX12_VIEWPORT to_viewport = CD3DX12_VIEWPORT(inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mToTextureUAV)), inData.mToTextureMip);
            const CD3DX12_VIEWPORT from_viewport = CD3DX12_VIEWPORT(inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mFromTextureSRV)), inData.mFromTextureMip);
            
            inCmdList.PushComputeConstants(BloomRootConstants
            {
                .mSrcTexture   = inResources.GetBindlessHeapIndex(inData.mFromTextureSRV),
                .mSrcMipLevel  = inData.mFromTextureMip,
                .mDstTexture   = inResources.GetBindlessHeapIndex(inData.mToTextureUAV),
                .mDstMipLevel  = inData.mToTextureMip,
                .mDispatchSize = UVec2(to_viewport.Width, to_viewport.Height),
                .mSrcSizeRcp   = Vec2(1.0f / from_viewport.Width, 1.0f / from_viewport.Height)
            });

            inCmdList->SetPipelineState(inPipeline);
            inCmdList->Dispatch(( to_viewport.Width + 7 ) / 8, ( to_viewport.Height + 7 ) / 8, 1);
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


    return inRenderGraph.AddComputePass<BloomPassData>("BLOOM PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, BloomPassData& inData)
    {
        const uint32_t cMipLevels = 6u;

        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format    = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width     = inRenderGraph.GetViewport().size.x,
            .height    = inRenderGraph.GetViewport().size.y,
            .mipLevels = cMipLevels,
            .usage     = Texture::SHADER_READ_WRITE,
            .debugName = "RT_BloomResult"
        });

        ioRGBuilder.Write(inData.mOutputTexture);

        // do an initial downsample from the current final scene texture to the first mip of the bloom chain
        AddDownsamplePass(inInputTexture, inData.mOutputTexture, 0, 1);

        // downsample along the bloom chain
        for (uint32_t mip = 1u; mip < cMipLevels - 1; mip++)
            AddDownsamplePass(inData.mOutputTexture, inData.mOutputTexture, mip, mip + 1);

        // upsample along the bloom chain
        for (uint32_t mip = cMipLevels - 1; mip > 0; mip--)
            AddUpsamplePass(inData.mOutputTexture, inData.mOutputTexture, mip, mip - 1);
    },

    [&inDevice](BloomPassData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        // clear or discard maybe?
    });
}



const DebugPrimitivesData& AddDebugOverlayPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inRenderTarget, RenderGraphResourceID inDepthTarget)
{
    constexpr size_t cPrimitiveBufferSize = 3 * UINT16_MAX;

    return inRenderGraph.AddGraphicsPass<DebugPrimitivesData>("DEBUG PRIMITIVES PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, DebugPrimitivesData& inData)
    {
        inData.mRenderTarget = ioRGBuilder.RenderTarget(inRenderTarget);
        inData.mDepthTarget = ioRGBuilder.DepthStencilTarget(inDepthTarget);

        ByteSlice vertex_shader, pixel_shader;
        g_SystemShaders.mDebugPrimitivesShader.GetGraphicsProgram(vertex_shader, pixel_shader);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inRenderPass->CreatePipelineStateDesc(inDevice, vertex_shader, pixel_shader);

        pso_state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso_state.RasterizerState.AntialiasedLineEnable = true;
        //pso_state.InputLayout = {};

        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(inData.mPipeline.GetAddressOf()));
        inData.mPipeline->SetName(L"PSO_DEBUG_PRIM_LINES");

        inRenderPass->ReserveMemory(cPrimitiveBufferSize);
    },
    [&inRenderGraph](DebugPrimitivesData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        Slice<const Vec4> line_vertices = g_DebugRenderer.GetLinesToRender();

        if (line_vertices.empty())
            return;

        const int size_in_bytes = glm::min(line_vertices.size_bytes(), cPrimitiveBufferSize);

        inRenderGraph.GetPerPassAllocator().AllocAndCopy(size_in_bytes, line_vertices.data(), inData.mLineVertexDataOffset);

        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.PushGraphicsConstants(DebugPrimitivesRootConstants { .mBufferOffset = inData.mLineVertexDataOffset });

        inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
        inCmdList->DrawInstanced(line_vertices.size(), 1, 0, 0);

        // restore triangle state for other passes
        inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
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
            .debugName = "RT_ComposeOutput"
        });

        inBuilder.RenderTarget(inData.mOutputTexture);

        inData.mInputTextureSRV = inBuilder.Read(inInputTexture);
        inData.mBloomTextureSRV = inBuilder.Read(inBloomTexture);

        ByteSlice vertex_shader, pixel_shader;
        g_SystemShaders.mFinalComposeShader.GetGraphicsProgram(vertex_shader, pixel_shader);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inRenderPass->CreatePipelineStateDesc(inDevice, vertex_shader, pixel_shader);

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

        ComposeRootConstants root_constants = ComposeRootConstants {
            .mBloomTexture = inResources.GetBindlessHeapIndex(inData.mBloomTextureSRV),
            .mInputTexture = inResources.GetBindlessHeapIndex(inData.mInputTextureSRV),
            .mSettings = {
                .mExposure = RenderSettings::mExposure,
                .mVignetteScale = RenderSettings::mVignetteScale,
                .mVignetteBias = RenderSettings::mVignetteBias,
                .mVignetteInner = RenderSettings::mVignetteInner,
                .mVignetteOuter = RenderSettings::mVignetteOuter,
                .mBloomBlendFactor = RenderSettings::mBloomBlendFactor,
                .mChromaticAberrationStrength = RenderSettings::mChromaticAberrationStrength
            }
        };

        root_constants.mSettings.mVignetteScale *= g_CVariables->GetValue<int>("r_enable_vignette");
        root_constants.mSettings.mBloomBlendFactor *= g_CVariables->GetValue<int>("r_enable_bloom");

        inCmdList.PushGraphicsConstants(root_constants);
        inCmdList->DrawInstanced(3, 1, 0, 0);
    });
}



const PreImGuiData& AddPreImGuiPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID ioDisplayTexture)
{
    return inRenderGraph.AddGraphicsPass<PreImGuiData>("TRANSITION TEXTURE PASS",

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
    const CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, draw_data->DisplaySize.x, draw_data->DisplaySize.y);
    inCmdList->RSSetViewports(1, &viewport);

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



const ImGuiData& AddImGuiPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inInputTexture, TextureID inBackBuffer)
{
    return inRenderGraph.AddGraphicsPass<ImGuiData>("IMGUI PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ImGuiData& inData)
    {
        constexpr size_t max_buffer_size = 65536; // Taken from Vulkan's ImGuiPass
        inData.mIndexScratchBuffer.resize(max_buffer_size);
        inData.mVertexScratchBuffer.resize(max_buffer_size);

        inData.mIndexBuffer = ioRGBuilder.Create(Buffer::Desc
        {
            .size   = max_buffer_size,
            .stride = sizeof(uint16_t),
            .usage  = Buffer::Usage::INDEX_BUFFER,
            .debugName = "ImGuiIndexBuffer"
        });

        inData.mVertexBuffer = ioRGBuilder.Create(Buffer::Desc
        {
            .size   = max_buffer_size,
            .stride = sizeof(ImDrawVert),
            .usage  = Buffer::Usage::VERTEX_BUFFER,
            .debugName = "ImGuiVertexBuffer"
        });

        inData.mInputTextureSRV = ioRGBuilder.Read(inInputTexture);
        inData.mBackBufferRTV = ioRGBuilder.RenderTarget(ioRGBuilder.Import(inDevice, inBackBuffer));

        static constexpr std::array input_layout =
        {
            D3D12_INPUT_ELEMENT_DESC { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ImDrawVert, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            D3D12_INPUT_ELEMENT_DESC { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ImDrawVert, uv), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            D3D12_INPUT_ELEMENT_DESC { "COLOR", 0, DXGI_FORMAT_R32_UINT, 0, offsetof(ImDrawVert, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        ByteSlice vertex_shader, pixel_shader;
        g_SystemShaders.mImGuiShader.GetGraphicsProgram(vertex_shader, pixel_shader);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inRenderPass->CreatePipelineStateDesc(inDevice, vertex_shader, pixel_shader);
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

    [&inRenderGraph, &inDevice, inBackBuffer](ImGuiData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        {   // manual barriers around the imported backbuffer resource, the rendergraph doesn't handle this kind of state
            auto backbuffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetD3D12Resource(inBackBuffer), D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
            inCmdList->ResourceBarrier(1, &backbuffer_barrier);
        }

        inCmdList->SetPipelineState(inData.mPipeline.Get());

        ImDrawData* draw_data = ImGui::GetDrawData();
        ImDrawIdx* idx_dst = (ImDrawIdx*)inData.mIndexScratchBuffer.data();
        ImDrawVert* vtx_dst = (ImDrawVert*)inData.mVertexScratchBuffer.data();

        for (const ImDrawList* cmd_list : draw_data->CmdLists)
        {
            memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtx_dst += cmd_list->VtxBuffer.Size;
            idx_dst += cmd_list->IdxBuffer.Size;
        }

        const int64_t index_buffer_size = (uint8_t*)idx_dst - inData.mIndexScratchBuffer.data();
        const int64_t vertex_buffer_size = (uint8_t*)vtx_dst - inData.mVertexScratchBuffer.data();

        assert(index_buffer_size < inData.mIndexScratchBuffer.size());
        assert(vertex_buffer_size < inData.mVertexScratchBuffer.size());

        int nr_of_barriers = 0;
        std::array barriers =
        {
            D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetBuffer(inData.mIndexBuffer).GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST)),
            D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetBuffer(inData.mVertexBuffer).GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST))
        };

        if (index_buffer_size) nr_of_barriers++;
        if (vertex_buffer_size) nr_of_barriers++;

        if (nr_of_barriers)
            inCmdList->ResourceBarrier(nr_of_barriers, barriers.data());

        if (vertex_buffer_size)
            inDevice.UploadBufferData(inCmdList, inDevice.GetBuffer(inData.mVertexBuffer), 0, inData.mVertexScratchBuffer.data(), vertex_buffer_size);

        if (index_buffer_size)
            inDevice.UploadBufferData(inCmdList, inDevice.GetBuffer(inData.mIndexBuffer), 0, inData.mIndexScratchBuffer.data(), index_buffer_size);

        if (nr_of_barriers)
        {
            for (D3D12_RESOURCE_BARRIER& barrier : barriers)
                std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

            inCmdList->ResourceBarrier(nr_of_barriers, barriers.data());
        }

        ImGuiRootConstants root_constants = { .mBindlessTextureIndex = 1 };
        ImGui_ImplDX12_SetupRenderState(draw_data, inCmdList, inDevice.GetBuffer(inData.mVertexBuffer), inDevice.GetBuffer(inData.mIndexBuffer), root_constants);

        int global_vtx_offset = 0;
        int global_idx_offset = 0;
        ImVec2 clip_off = draw_data->DisplayPos;

        for (const ImDrawList* cmd_list : draw_data->CmdLists)
        {
            for (const ImDrawCmd& cmd : cmd_list->CmdBuffer)
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
                    const ImVec2 clip_min = ImVec2(cmd.ClipRect.x - clip_off.x, cmd.ClipRect.y - clip_off.y);
                    const ImVec2 clip_max = ImVec2(cmd.ClipRect.z - clip_off.x, cmd.ClipRect.w - clip_off.y);
                    if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                        continue;

                    const D3D12_RECT scissor_rect = D3D12_RECT { (LONG)clip_min.x, (LONG)clip_min.y, (LONG)clip_max.x, (LONG)clip_max.y };
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
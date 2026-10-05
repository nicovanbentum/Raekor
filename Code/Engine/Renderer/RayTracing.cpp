#include "PCH.h"
#include "RayTracing.h"

#include "Scene.h"
#include "Shader.h"
#include "GPUProfiler.h"
#include "RenderPasses.h"

#include "Camera.h"
#include "Primitives.h"

namespace RK::DX12 {

const BuildAccelerationStructuresData& AddBuildAccelerationStructuresPass(RenderGraph& inRenderGraph, Device& inDevice, RayTracedScene& inScene)
{
    return inRenderGraph.AddComputePass<BuildAccelerationStructuresData>("Build Acceleration Structures",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, BuildAccelerationStructuresData& inData)
    {
    },
    [&inDevice, &inScene](BuildAccelerationStructuresData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        static const int& update_skinning = g_CVariables->Create("update_skinning", 1, true);

        const D3D12_RESOURCE_BARRIER uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);

        if (update_skinning)
        {
            for (const auto& [entity, mesh, skeleton] : inScene->Each<Mesh, Skeleton>())
            {
                if (mesh.HasBLAS() && skeleton.gpuBuffersUploaded)
                    inScene.UpdateBLAS(nullptr, inDevice, mesh, skeleton, inCmdList);
            }
        }

        inCmdList->ResourceBarrier(1, &uav_barrier);

        inScene.BuildTLAS(inDevice, inCmdList);

        inCmdList->ResourceBarrier(1, &uav_barrier);
    });
}



const RenderGraphResourceID AddRayTracedShadowsPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferOutput& inGBuffer)
{
    static constexpr int cTileSize = RT_SHADOWS_GROUP_DIM;
    static constexpr int cPackedRaysWidth = RT_SHADOWS_PACKED_DIM_X;
    static constexpr int cPackedRaysHeight = RT_SHADOWS_PACKED_DIM_Y;

    auto TraceShadowRaysPass = [](RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferOutput& inGBuffer)
    {
        return inRenderGraph.AddComputePass<TraceShadowTilesData>("RT Shadows Trace",
        [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, TraceShadowTilesData& inData)
        {
            inData.mOutputTexture = inRGBuilder.Create(Texture::Desc
            {
                .format = DXGI_FORMAT_R32_UINT,
                .width  = (inRenderGraph.GetViewport().GetRenderSize().x + cPackedRaysWidth - 1 ) / cPackedRaysWidth,
                .height = (inRenderGraph.GetViewport().GetRenderSize().y + cPackedRaysHeight - 1) / cPackedRaysHeight,
                .usage  = Texture::Usage::SHADER_READ_WRITE,
                .debugName = "RT_PackedShadowRayHits"
            });

            inRGBuilder.Write(inData.mOutputTexture);
            inData.mGBufferDepthTextureSRV = inRGBuilder.Read(inGBuffer.mDepthTexture);
            inData.mGBufferRenderTextureSRV = inRGBuilder.Read(inGBuffer.mRenderTexture);
        },

        [&inRenderGraph, &inDevice, &inScene](TraceShadowTilesData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
        {
            if (!inScene.HasTLAS())
                return;

            if (!inScene->GetSunLight())
                return;

            const Viewport& viewport = inRenderGraph.GetViewport();
            const Texture& texture  = inDevice.GetTexture(inResources.GetTexture(inData.mOutputTexture));

            inCmdList.PushComputeConstants(ShadowMaskRootConstants
            {
                .mShadowMaskTexture = inResources.GetBindlessHeapIndex(inData.mOutputTexture),
                .mGbufferDepthTexture = inResources.GetBindlessHeapIndex(inData.mGBufferDepthTextureSRV),
                .mGbufferRenderTexture = inResources.GetBindlessHeapIndex(inData.mGBufferRenderTextureSRV),
                .mDispatchSize = viewport.GetRenderSize()
            });

            inCmdList->SetPipelineState(g_SystemShaders.mTraceShadowRaysShader.GetComputePSO());
            inCmdList->Dispatch(texture.GetWidth(), texture.GetHeight(), 1);
        });
    };

    auto ClearShadowTilesPass = [](RenderGraph& inRenderGraph, Device& inDevice)
    {
        return inRenderGraph.AddComputePass<ClearShadowTilesData>("RT Shadows Clear Tiles",
        [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, ClearShadowTilesData& inData)
        {
            const UVec2& render_size = inRenderGraph.GetViewport().GetRenderSize();
            const uint32_t tile_width = ( render_size.x + cTileSize - 1 ) / cTileSize;
            const uint32_t tile_height = ( render_size.y + cTileSize - 1 ) / cTileSize;
            const uint32_t tile_buffer_size = tile_width * tile_height * sizeof(uint32_t);

            inData.mTilesBuffer = inRGBuilder.Create(Buffer::RWStructuredBuffer(tile_buffer_size, sizeof(uint32_t), "ShadowTilesBuffer"));
            inData.mIndirectDispatchBuffer = inRGBuilder.Create(Buffer::RWByteAddressBuffer(sizeof(D3D12_DISPATCH_ARGUMENTS), "ShadowsDispatchBuffer"));

            inRGBuilder.Write(inData.mTilesBuffer);
            inRGBuilder.Write(inData.mIndirectDispatchBuffer);
        },

        [&inRenderGraph, &inDevice](ClearShadowTilesData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
        {
            inCmdList.PushComputeConstants(ShadowsClearRootConstants
            {
                .mTilesBuffer = inResources.GetBindlessHeapIndex(inData.mTilesBuffer),
                .mDispatchBuffer = inResources.GetBindlessHeapIndex(inData.mIndirectDispatchBuffer)
            });

            const Buffer& tiles_buffer = inDevice.GetBuffer(inResources.GetBuffer(inData.mTilesBuffer));
            
            inCmdList->SetPipelineState(g_SystemShaders.mClearShadowTilesShader.GetComputePSO());
            inCmdList->Dispatch(( (tiles_buffer.GetSize() / sizeof(uint32_t)) + 63 ) / 64, 1, 1);
        });
    };

    auto ClassifyShadowTilesPass = [](RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const TraceShadowTilesData& inTraceData, const ClearShadowTilesData& inClearData)
    {
        return inRenderGraph.AddComputePass<ClassifyShadowTilesData>("RT Shadows Classify",
        [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, ClassifyShadowTilesData& inData)
        {
            const UVec2& render_size = inRenderGraph.GetViewport().GetRenderSize();
            const uint32_t tile_width   = ( render_size.x + cTileSize - 1 ) / cTileSize;
            const uint32_t tile_height  = ( render_size.y + cTileSize - 1 ) / cTileSize;

            inData.mTilesBufferUAV              = inRGBuilder.Write(inClearData.mTilesBuffer);
            inData.mIndirectDispatchBufferUAV   = inRGBuilder.Write(inClearData.mIndirectDispatchBuffer);
            inData.mTracedShadowRaysTextureSRV  = inRGBuilder.Read(inTraceData.mOutputTexture);
        },

        [&inRenderGraph, &inDevice, &inScene](ClassifyShadowTilesData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
        {
            const Viewport& viewport = inRenderGraph.GetViewport();
            const UVec2 dispatch_size = UVec2((viewport.GetRenderSize().x + cTileSize - 1) / cTileSize, (viewport.GetRenderSize().y + cTileSize - 1) / cTileSize);

            inCmdList.PushComputeConstants(ShadowsClassifyRootConstants 
            { 
                .mShadowMaskTexture = inResources.GetBindlessHeapIndex(inData.mTracedShadowRaysTextureSRV),
                .mTilesBuffer       = inResources.GetBindlessHeapIndex(inData.mTilesBufferUAV),
                .mDispatchBuffer    = inResources.GetBindlessHeapIndex(inData.mIndirectDispatchBufferUAV),
                .mDispatchSize      = viewport.GetRenderSize()
            });

            inCmdList->SetPipelineState(g_SystemShaders.mClassifyShadowTilesShader.GetComputePSO());
            inCmdList->Dispatch(dispatch_size.x, dispatch_size.y, 1);
        });
    };

    auto ClearShadowsPass = [](RenderGraph& inRenderGraph, Device& inDevice)
    {
        return inRenderGraph.AddComputePass<ClearShadowsData>("RT Shadows Clear",
        [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, ClearShadowsData& inData)
        {
            inData.mShadowsTexture = inRGBuilder.Create(Texture::Desc
            {
                .format = DXGI_FORMAT_R32G32_FLOAT,
                .width  = inRenderGraph.GetViewport().GetRenderSize().x,
                .height = inRenderGraph.GetViewport().GetRenderSize().y,
                .usage  = Texture::Usage::SHADER_READ_WRITE,
                .debugName = "RT_ShadowMask"
            });

            inData.mShadowsTextureHistory = inRGBuilder.Create(Texture::Desc
            {
                .format = DXGI_FORMAT_R32G32_FLOAT,
                .width  = inRenderGraph.GetViewport().GetRenderSize().x,
                .height = inRenderGraph.GetViewport().GetRenderSize().y,
                .usage  = Texture::Usage::SHADER_READ_WRITE,
                .debugName = "RT_ShadowMaskHistory"
            });

            inRGBuilder.Write(inData.mShadowsTexture);
            inRGBuilder.Write(inData.mShadowsTextureHistory);
        },

        [&inRenderGraph, &inDevice](ClearShadowsData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
        {
            ClearTextureUAV(inDevice, inResources.GetTexture(inData.mShadowsTexture), Vec4(1.0f), inCmdList);
        });
    };

    auto DenoiseShadowsPass = [](RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferOutput& inGBuffer, const TraceShadowTilesData& inTraceData, const ClearShadowTilesData& inTilesData, const ClearShadowsData& inShadowsData)
    {
        return inRenderGraph.AddComputePass<DenoiseShadowsData>("RT Shadows Denoise",
        [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, DenoiseShadowsData& inData)
        {
            inData.mOutputTextureUAV = inRGBuilder.Write(inShadowsData.mShadowsTexture);
            inData.mHistoryTextureUAV = inRGBuilder.Write(inShadowsData.mShadowsTextureHistory);

            inData.mDepthTextureSRV = inRGBuilder.Read(inGBuffer.mDepthTexture);
            inData.mGBufferTextureSRV = inRGBuilder.Read(inGBuffer.mRenderTexture);
            inData.mVelocityTextureSRV = inRGBuilder.Read(inGBuffer.mVelocityTexture);
            inData.mSelectionTextureSRV = inRGBuilder.Read(inGBuffer.mSelectionTexture);

            inData.mTracedShadowRaysTextureSRV = inRGBuilder.Read(inTraceData.mOutputTexture);

            inData.mTilesBufferSRV = inRGBuilder.Read(inTilesData.mTilesBuffer);
            inData.mDenoisedTilesBufferSRV = inRGBuilder.Read(inTilesData.mTilesBuffer);

            // don't actually need the views, just barriers and render graph resource IDs
            inData.mIndirectDispatchBufferSRV = inRGBuilder.ReadIndirectArgs(inTilesData.mIndirectDispatchBuffer);
            inData.mDenoisedIndirectDispatchBufferSRV = inRGBuilder.ReadIndirectArgs(inTilesData.mIndirectDispatchBuffer);
        },

        [&inRenderGraph, &inDevice, &inScene](DenoiseShadowsData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
        {
            if (!inScene.HasTLAS())
                return;

            if (!inScene->GetSunLight())
                return;

            ShadowsDenoiseRootConstants root_constants =
            {
                .mResultTexture     = inResources.GetBindlessHeapIndex(inData.mOutputTextureUAV),
                .mHistoryTexture    = inResources.GetBindlessHeapIndex(inData.mHistoryTextureUAV),
                .mDepthTexture      = inResources.GetBindlessHeapIndex(inData.mDepthTextureSRV),
                .mVelocityTexture   = inResources.GetBindlessHeapIndex(inData.mVelocityTextureSRV),
                .mShadowMaskTexture = inResources.GetBindlessHeapIndex(inData.mTracedShadowRaysTextureSRV),
                .mSelectionTexture  = inResources.GetBindlessHeapIndex(inData.mSelectionTextureSRV),
                .mDispatchSize      = inRenderGraph.GetViewport().GetRenderSize()
            };

            //{
            //    // write black to the final shadow texture for all the tiles that didn't need denoising
            //    Buffer& dispatch_buffer = inResources.GetBuffer(inData.mIndirectDispatchBufferSRV);
            //    inCmdList->ExecuteIndirect(inDevice.GetCommandSignature(COMMAND_SIGNATURE_DISPATCH), 1, inDevice.GetD3D12Resource(dispatch_buffer), 0, nullptr, 0);
            //}

            {
                // write all the denoised tiles to the final shadow texture
                root_constants.mTilesBuffer = inResources.GetBindlessHeapIndex(inData.mDenoisedTilesBufferSRV);
                inCmdList.PushComputeConstants(root_constants);

                inCmdList->SetPipelineState(g_SystemShaders.mDenoiseShadowTilesShader.GetComputePSO());

                BufferID dispatch_buffer = inResources.GetBufferView(inData.mDenoisedIndirectDispatchBufferSRV);
                inCmdList->ExecuteIndirect(inDevice.GetCommandSignature(COMMAND_SIGNATURE_DISPATCH), 1, inDevice.GetD3D12Resource(dispatch_buffer), 0, nullptr, 0);
            }

            ID3D12Resource* result_texture_resource = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mOutputTextureUAV));
            ID3D12Resource* history_texture_resource = inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mHistoryTextureUAV));

            std::array barriers =
            {
                D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(result_texture_resource, GetD3D12ResourceStates(Texture::SHADER_READ_WRITE), D3D12_RESOURCE_STATE_COPY_SOURCE)),
                D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(history_texture_resource, GetD3D12ResourceStates(Texture::SHADER_READ_WRITE), D3D12_RESOURCE_STATE_COPY_DEST))
            };
            inCmdList->ResourceBarrier(barriers.size(), barriers.data());

            const CD3DX12_TEXTURE_COPY_LOCATION dest = CD3DX12_TEXTURE_COPY_LOCATION(history_texture_resource, 0);
            const CD3DX12_TEXTURE_COPY_LOCATION source = CD3DX12_TEXTURE_COPY_LOCATION(result_texture_resource, 0);
            inCmdList->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);

            for (D3D12_RESOURCE_BARRIER& barrier : barriers)
                std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

            inCmdList->ResourceBarrier(barriers.size(), barriers.data());
        });
    };

    const TraceShadowTilesData& traced_rays_data = TraceShadowRaysPass(inRenderGraph, inDevice, inScene, inGBuffer);

    const ClearShadowTilesData& clear_tiles_data = ClearShadowTilesPass(inRenderGraph, inDevice);

    const ClassifyShadowTilesData& classify_data = ClassifyShadowTilesPass(inRenderGraph, inDevice, inScene, traced_rays_data, clear_tiles_data);

    const ClearShadowsData& clear_shadows_data   = ClearShadowsPass(inRenderGraph, inDevice);

    const DenoiseShadowsData& denoise_tiles_data = DenoiseShadowsPass(inRenderGraph, inDevice, inScene, inGBuffer, traced_rays_data, clear_tiles_data, clear_shadows_data);

    return clear_shadows_data.mShadowsTexture;
}



const RTAOData& AddAmbientOcclusionPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferOutput& inGBuffer)
{
    return inRenderGraph.AddComputePass<RTAOData>("RTAO",
    [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, RTAOData& inData)
    {
        inData.mOutputTexture = inRGBuilder.Create(Texture::Desc{
            .format = DXGI_FORMAT_R32_FLOAT,
            .width  = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = "RT_AOMask"
        });

        inData.mHistoryTexture = inRGBuilder.Create(Texture::Desc{
            .format = DXGI_FORMAT_R32_FLOAT,
            .width  = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage  = Texture::Usage::SHADER_READ_ONLY,
            .debugName = "RT_AOMaskHistory"
        });

        inRGBuilder.Write(inData.mOutputTexture);

        inData.mHistoryTextureSRV = inRGBuilder.Read(inData.mHistoryTexture);
        inData.mGbufferDepthTextureSRV = inRGBuilder.Read(inGBuffer.mDepthTexture);
        inData.mGBufferRenderTextureSRV = inRGBuilder.Read(inGBuffer.mRenderTexture);
        inData.mGBufferVelocityTextureSRV = inRGBuilder.Read(inGBuffer.mVelocityTexture);
    },

    [&inRenderGraph, &inDevice, &inScene](RTAOData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        if (!inScene.HasTLAS())
            return;

        const Viewport& viewport = inRenderGraph.GetViewport();

        inCmdList.PushComputeConstants(AmbientOcclusionRootConstants
        {
            .mAOmaskTexture           = inResources.GetBindlessHeapIndex(inData.mOutputTexture),
            .mAOmaskHistoryTexture    = inResources.GetBindlessHeapIndex(inData.mHistoryTexture),
            .mGbufferDepthTexture     = inResources.GetBindlessHeapIndex(inData.mGbufferDepthTextureSRV),
            .mGbufferRenderTexture    = inResources.GetBindlessHeapIndex(inData.mGBufferRenderTextureSRV),
            .mGbufferVelocityTexture  = inResources.GetBindlessHeapIndex(inData.mGBufferVelocityTextureSRV),
            .mDispatchSize            = viewport.GetRenderSize(),
            .mParams = AmbientOcclusionParams {
                .mRadius = RenderSettings::mRTAORadius,
                .mPower  = RenderSettings::mRTAOPower,
                .mNormalBias  = RenderSettings::mRTAONormalBias,
                .mSampleCount = RenderSettings::mRTAOSampleCount
            },
        });

        inCmdList->SetPipelineState(g_SystemShaders.mRTAmbientOcclusionShader.GetComputePSO());
        inCmdList->Dispatch((viewport.GetRenderSize().x + 7) / 8, (viewport.GetRenderSize().y + 7) / 8, 1);

        ID3D12Resource* result_texture_resource = inDevice.GetD3D12Resource(inResources.GetTexture(inData.mOutputTexture));
        ID3D12Resource* history_texture_resource = inDevice.GetD3D12Resource(inResources.GetTexture(inData.mHistoryTexture));

        std::array barriers =
        {
            D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(result_texture_resource, GetD3D12ResourceStates(Texture::SHADER_READ_WRITE), D3D12_RESOURCE_STATE_COPY_SOURCE)),
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



const ReflectionsData& AddReflectionsPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferOutput& inGBuffer, const SkyCubeData& inSkyCubeData)
{
    return inRenderGraph.AddComputePass<ReflectionsData>("RT Specular",
        [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, ReflectionsData& inData)
    {
        inData.mOutputTexture = inRGBuilder.Create(Texture::Desc
            {
                .format    = DXGI_FORMAT_R16G16B16A16_FLOAT,
                .width     = inRenderGraph.GetViewport().GetRenderSize().x,
                .height    = inRenderGraph.GetViewport().GetRenderSize().y,
                .mipLevels = 0, // let it calculate the nr of mips
                .usage     = Texture::Usage::SHADER_READ_WRITE,
                .debugName = "RT_Reflections"
            });

        inRGBuilder.Write(inData.mOutputTexture);

        inData.mSkyCubeTextureSRV = inRGBuilder.Read(inSkyCubeData.mSkyCubeTexture);
        inData.mGBufferDepthTextureSRV = inRGBuilder.Read(inGBuffer.mDepthTexture);
        inData.mGbufferRenderTextureSRV = inRGBuilder.Read(inGBuffer.mRenderTexture);
    },

    [&inRenderGraph, &inDevice, &inScene](ReflectionsData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        if (!inScene.HasTLAS())
            return;

        const Viewport& viewport = inRenderGraph.GetViewport();

        inCmdList.PushComputeConstants(ReflectionsRootConstants {
            .mResultTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mOutputTexture)),
            .mSkyCubeTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mSkyCubeTextureSRV)),
            .mGbufferDepthTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mGBufferDepthTextureSRV)),
            .mGbufferRenderTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mGbufferRenderTextureSRV)),
            .mDispatchSize = viewport.GetRenderSize()
            });

        inCmdList->SetPipelineState(g_SystemShaders.mRTReflectionsShader.GetComputePSO());
        inCmdList->Dispatch(( viewport.GetRenderSize().x + 7 ) / 8, ( viewport.GetRenderSize().y + 7 ) / 8, 1);
    });
}



const PathTraceData& AddPathTracePass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const SkyCubeData& inSkyCubeData, GBufferOutput& ioGBuffer)
{
    return inRenderGraph.AddComputePass<PathTraceData>("PathTrace",
    [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, PathTraceData& inData)
    {
        inData.mOutputTexture = inRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage  = Texture::SHADER_READ_WRITE,
            .debugName = "RT_GBufferCompressed"
        });

        inData.mGBufferTexture = inRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = "RT_PathTraceNormals"
        });

        inData.mAccumulationTexture = inRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = "RT_PathTraceAccumulation"
        });

        inData.mSelectionTexture = inRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32_UINT,
            .width  = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = "RT_GBufferSelection"
        });

        inData.mDepthTexture = inRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_D32_FLOAT,
            .width  = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage  = Texture::DEPTH_STENCIL_TARGET,
            .debugName = "RT_GBufferDepth"
        });

        inData.mDepthWriteTexture = inRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32_FLOAT,
            .width = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage = Texture::SHADER_READ_WRITE,
            .debugName = "RT_DepthWrite"
        });

        ioGBuffer.mDepthTexture = inData.mDepthTexture;
        ioGBuffer.mRenderTexture = inData.mGBufferTexture;
        ioGBuffer.mSelectionTexture = inData.mSelectionTexture;

        inData.mSkyCubeTextureSRV = inRGBuilder.Read(inSkyCubeData.mSkyCubeTexture);
    },

    [&inRenderGraph, &inDevice, &inScene](PathTraceData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        const Viewport& viewport = inRenderGraph.GetViewport();

        PathTraceRootConstants constants =
        {
            .mReset = RenderSettings::mPathTraceReset,
            .mBounces = RenderSettings::mPathTraceBounces,
            .mAlphaBounces = RenderSettings::mPathTraceAlphaBounces,
            .mResultTexture = inDevice.GetBindlessHeapIndex(inResources.GetTexture(inData.mOutputTexture)),
            .mAccumulationTexture = inDevice.GetBindlessHeapIndex(inResources.GetTexture(inData.mAccumulationTexture)),
            .mSelectionTexture = inDevice.GetBindlessHeapIndex(inResources.GetTexture(inData.mSelectionTexture)),
            .mDepthTexture = inDevice.GetBindlessHeapIndex(inResources.GetTexture(inData.mDepthWriteTexture)),
            .mSkyCubeTexture = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mSkyCubeTextureSRV)),
            .mGBufferTexture = inDevice.GetBindlessHeapIndex(inResources.GetTexture(inData.mGBufferTexture)),
            .mDispatchSize = viewport.GetRenderSize(),
        };

        inCmdList.PushComputeConstants(constants);

        inCmdList->SetPipelineState(g_SystemShaders.mRTPathTraceShader.GetComputePSO());
        inCmdList->Dispatch(( viewport.GetRenderSize().x + 7 ) / 8, ( viewport.GetRenderSize().y + 7 ) / 8, 1);

        RenderSettings::mPathTraceReset = false;

        ID3D12Resource* depth_texture = inDevice.GetD3D12Resource(inResources.GetTexture(inData.mDepthTexture));
        ID3D12Resource* depth_write_texture = inDevice.GetD3D12Resource(inResources.GetTexture(inData.mDepthWriteTexture));

        std::array barriers =
        {
            D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(depth_texture, GetD3D12ResourceStates(Texture::DEPTH_STENCIL_TARGET), D3D12_RESOURCE_STATE_COPY_DEST)),
            D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(depth_write_texture, GetD3D12ResourceStates(Texture::SHADER_READ_WRITE), D3D12_RESOURCE_STATE_COPY_SOURCE))
        };
        inCmdList->ResourceBarrier(barriers.size(), barriers.data());

        inCmdList->CopyResource(depth_texture, depth_write_texture);

        for (D3D12_RESOURCE_BARRIER& barrier : barriers)
            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

        inCmdList->ResourceBarrier(barriers.size(), barriers.data());
    });
}



DDGIOutput AddDDGIPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferOutput& inGBuffer, const SkyCubeData& inSkyCubeData)
{
    //////////////////////////////////////////
    ///// Probe Trace Compute Pass
    //////////////////////////////////////////
    struct ProbeTraceData
    {
        RenderGraphResourceID mProbeDataBuffer;
        RenderGraphResourceViewID mProbeDataBufferSRV;
        RenderGraphResourceID mRaysDepthTexture;
        RenderGraphResourceID mProbesDepthTexture;
        RenderGraphResourceViewID mProbesDepthTextureSRV;
        RenderGraphResourceID mRaysIrradianceTexture;
        RenderGraphResourceID mProbesIrradianceTexture;
        RenderGraphResourceViewID mProbesIrradianceTextureSRV;
        RenderGraphResourceViewID mSkyCubeTextureSRV;
        Mat4x4 mRandomRotationMatrix;
    };

    const ProbeTraceData& trace_data = inRenderGraph.AddComputePass<ProbeTraceData>("DDGI Trace",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ProbeTraceData& inData)
    {
        const IVec3 probe_count = RenderSettings::mDDGIProbeCount;
        const int total_probe_count = RenderSettings::mDDGIProbeCount.x * RenderSettings::mDDGIProbeCount.y * RenderSettings::mDDGIProbeCount.z;

        inData.mProbeDataBuffer = ioRGBuilder.Create(Buffer::Desc 
        {
            .size   = total_probe_count * sizeof(ProbeData),
            .stride = sizeof(ProbeData),
            .usage  = Buffer::Usage::SHADER_READ_WRITE,
            .debugName = "DDGI_ProbeData"
        });

        inData.mRaysDepthTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16_FLOAT,
            .width  = DDGI_RAYS_PER_PROBE,
            .height = uint32_t(total_probe_count),
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = "DDGI_TracedDepth"
        });

        inData.mRaysIrradianceTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R11G11B10_FLOAT,
            .width  = DDGI_RAYS_PER_PROBE,
            .height = uint32_t(total_probe_count),
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = "DDGI_TracedIrradiance"
        });

        inData.mProbesDepthTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16_FLOAT,
            .width  = uint32_t(DDGI_DEPTH_TEXELS * DDGI_PROBES_PER_ROW),
            .height = uint32_t(DDGI_DEPTH_TEXELS * std::max((total_probe_count + DDGI_PROBES_PER_ROW - 1) / DDGI_PROBES_PER_ROW, 1)),
            .usage  = Texture::Usage::SHADER_READ_ONLY,
            .debugName = "DDGI_UpdatedDepth"
        });

        inData.mProbesIrradianceTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = uint32_t(DDGI_IRRADIANCE_TEXELS * DDGI_PROBES_PER_ROW),
            .height = uint32_t(DDGI_IRRADIANCE_TEXELS * std::max((total_probe_count + DDGI_PROBES_PER_ROW - 1) / DDGI_PROBES_PER_ROW, 1)),
            .usage  = Texture::Usage::SHADER_READ_ONLY,
            .debugName = "DDGI_UpdatedIrradiance"
        });

        inData.mProbeDataBufferSRV = ioRGBuilder.Read(inData.mProbeDataBuffer);
        inData.mProbesDepthTextureSRV = ioRGBuilder.Read(inData.mProbesDepthTexture);
        inData.mProbesIrradianceTextureSRV = ioRGBuilder.Read(inData.mProbesIrradianceTexture);

        ioRGBuilder.Write(inData.mRaysDepthTexture);
        ioRGBuilder.Write(inData.mRaysIrradianceTexture);

        inData.mSkyCubeTextureSRV = ioRGBuilder.Read(inSkyCubeData.mSkyCubeTexture);
    },
    [&inRenderGraph, &inDevice, &inScene](ProbeTraceData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        if (!inScene.HasTLAS())
            return;

        //if (ProbeUpdateData::mClear)
        //{
        //    ClearTextureUAV(inDevice, inRGResources.GetTexture(inData.mProbesIrradianceTexture), Vec4(0.0), inCmdList);
        //    ProbeUpdateData::mClear = false;
        //}

        auto Index3Dto1D = [](UVec3 inIndex, UVec3 inCount)
        {
            return inIndex.x + inIndex.y * inCount.x + inIndex.z * inCount.x * inCount.y;
        };

        inData.mRandomRotationMatrix = gRandomOrientation();

        ProbeTraceRootConstants root_constants =
        {
            .mDebugProbeIndex = Index3Dto1D(RenderSettings::mDDGIDebugProbe, RenderSettings::mDDGIProbeCount),
            .mSkyCubeTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mSkyCubeTextureSRV)),
            .mRandomRotationMatrix = inData.mRandomRotationMatrix,
            .mDDGIData = DDGIData {
                .mProbeCount = RenderSettings::mDDGIProbeCount,
                .mProbeRadius = RenderSettings::mDDGIDebugRadius,
                .mProbeSpacing = RenderSettings::mDDGIProbeSpacing,
                .mUseChebyshev = RenderSettings::mDDGIUseChebyshev,
                .mCornerPosition = RenderSettings::mDDGICornerPosition,
                .mProbesDataBuffer = inRGResources.GetBindlessHeapIndex(inData.mProbeDataBufferSRV),
                .mRaysDepthTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mRaysDepthTexture)),
                .mProbesDepthTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mProbesDepthTextureSRV)),
                .mRaysIrradianceTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mRaysIrradianceTexture)),
                .mProbesIrradianceTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mProbesIrradianceTextureSRV))
            }
        };
        
        inCmdList.PushComputeConstants(root_constants);

        const int total_probe_count = root_constants.mDDGIData.mProbeCount.x * root_constants.mDDGIData.mProbeCount.y * root_constants.mDDGIData.mProbeCount.z;

        inCmdList->SetPipelineState(g_SystemShaders.mProbeTraceShader.GetComputePSO());
        inCmdList->Dispatch(DDGI_RAYS_PER_PROBE / DDGI_TRACE_SIZE, total_probe_count, 1);
    });



    //////////////////////////////////////////
    ///// Probe Update Compute Pass
    //////////////////////////////////////////
    struct ProbeUpdateData
    {
        RenderGraphResourceViewID mProbesBufferUAV;
        RenderGraphResourceViewID mProbesDepthTextureUAV;
        RenderGraphResourceViewID mProbesIrradianceTextureUAV;
        RenderGraphResourceViewID mRaysDepthTextureSRV;
        RenderGraphResourceViewID mRaysIrradianceTextureSRV;
    };

    const ProbeUpdateData& update_data =  inRenderGraph.AddComputePass<ProbeUpdateData>("DDGI Update",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ProbeUpdateData& inData)
    {
        inData.mProbesBufferUAV = ioRGBuilder.Write(trace_data.mProbeDataBuffer);
        inData.mProbesDepthTextureUAV = ioRGBuilder.Write(trace_data.mProbesDepthTexture);
        inData.mProbesIrradianceTextureUAV = ioRGBuilder.Write(trace_data.mProbesIrradianceTexture);

        inData.mRaysDepthTextureSRV = ioRGBuilder.Read(trace_data.mRaysDepthTexture);
        inData.mRaysIrradianceTextureSRV = ioRGBuilder.Read(trace_data.mRaysIrradianceTexture);
    },

    [&inDevice, &trace_data, &inScene](ProbeUpdateData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        if (!inScene.HasTLAS())
            return;

        inCmdList.PushComputeConstants(ProbeUpdateRootConstants
        {
            .mRandomRotationMatrix = trace_data.mRandomRotationMatrix,
            .mDDGIData = DDGIData {
                .mProbeCount = RenderSettings::mDDGIProbeCount,
                .mProbeRadius = RenderSettings::mDDGIDebugRadius,
                .mProbeSpacing = RenderSettings::mDDGIProbeSpacing,
                .mUseChebyshev = RenderSettings::mDDGIUseChebyshev,
                .mCornerPosition = RenderSettings::mDDGICornerPosition,
                .mProbesDataBuffer = inResources.GetBindlessHeapIndex(inData.mProbesBufferUAV),
                .mRaysDepthTexture = inResources.GetBindlessHeapIndex(inData.mRaysDepthTextureSRV),
                .mProbesDepthTexture = inResources.GetBindlessHeapIndex(inData.mProbesDepthTextureUAV),
                .mRaysIrradianceTexture = inResources.GetBindlessHeapIndex(inData.mRaysIrradianceTextureSRV),
                .mProbesIrradianceTexture = inResources.GetBindlessHeapIndex(inData.mProbesIrradianceTextureUAV)
            }
        });

        const int total_probe_count = RenderSettings::mDDGIProbeCount.x * RenderSettings::mDDGIProbeCount.y * RenderSettings::mDDGIProbeCount.z;

#if 1
        {
            PROFILE_SCOPE_GPU(inCmdList, "Update Probes");

            inCmdList->SetPipelineState(g_SystemShaders.mProbeUpdateShader.GetComputePSO());
            inCmdList->Dispatch((total_probe_count + 63) / 64 , 1, 1);

            const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(inDevice.GetD3D12Resource(inResources.GetBufferView(inData.mProbesBufferUAV)));
            inCmdList->ResourceBarrier(1, &barrier);
        }

        {
            PROFILE_SCOPE_GPU(inCmdList, "Update Depth");

            inCmdList->SetPipelineState(g_SystemShaders.mProbeUpdateDepthShader.GetComputePSO());
            const Texture& depth_texture = inDevice.GetTexture(inResources.GetTextureView(inData.mProbesDepthTextureUAV));
            inCmdList->Dispatch(depth_texture.GetDesc().width / DDGI_DEPTH_TEXELS, depth_texture.GetDesc().height / DDGI_DEPTH_TEXELS, 1);
        }
#endif

        {
            PROFILE_SCOPE_GPU(inCmdList, "Update Irradiance");

            inCmdList->SetPipelineState(g_SystemShaders.mProbeUpdateIrradianceShader.GetComputePSO());
            const Texture& irradiance_texture = inDevice.GetTexture(inResources.GetTextureView(inData.mProbesIrradianceTextureUAV));
            inCmdList->Dispatch(irradiance_texture.GetWidth() / DDGI_IRRADIANCE_TEXELS, irradiance_texture.GetHeight() / DDGI_IRRADIANCE_TEXELS, 1);
        }
    });

    //////////////////////////////////////////
    ///// Probe Sample Compute Pass
    //////////////////////////////////////////
    struct ProbeSampleData
    {
        RenderGraphResourceID mOutputTexture;
        RenderGraphResourceViewID mDepthTextureSRV;
        RenderGraphResourceViewID mGBufferTextureSRV;
        RenderGraphResourceViewID mProbeDataBufferSRV;
        RenderGraphResourceViewID mProbesDepthTextureSRV;
        RenderGraphResourceViewID mProbesIrradianceTextureSRV;
    };

    const ProbeSampleData& sample_data = inRenderGraph.AddComputePass<ProbeSampleData>("DDGI Sample",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ProbeSampleData& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage  = Texture::SHADER_READ_WRITE,
            .debugName = "RT_DDGISampleOutput"
        });

        ioRGBuilder.Write(inData.mOutputTexture);

        inData.mDepthTextureSRV = ioRGBuilder.Read(inGBuffer.mDepthTexture);
        inData.mGBufferTextureSRV = ioRGBuilder.Read(inGBuffer.mRenderTexture);
        inData.mProbeDataBufferSRV = ioRGBuilder.Read(trace_data.mProbeDataBuffer);
        inData.mProbesDepthTextureSRV = ioRGBuilder.Read(trace_data.mProbesDepthTexture);
        inData.mProbesIrradianceTextureSRV = ioRGBuilder.Read(trace_data.mProbesIrradianceTexture);
    },

    [&inDevice](ProbeSampleData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        const Texture& output_texture = inDevice.GetTexture(inResources.GetTexture(inData.mOutputTexture));

        ProbeSampleRootConstants root_constants = ProbeSampleRootConstants
        {
            .mDDGIData = DDGIData {
                .mProbeCount = RenderSettings::mDDGIProbeCount,
                .mProbeRadius = RenderSettings::mDDGIDebugRadius,
                .mProbeSpacing = RenderSettings::mDDGIProbeSpacing,
                .mUseChebyshev = RenderSettings::mDDGIUseChebyshev,
                .mCornerPosition = RenderSettings::mDDGICornerPosition,
                .mProbesDataBuffer = inResources.GetBindlessHeapIndex(inData.mProbeDataBufferSRV),
                .mProbesDepthTexture = inResources.GetBindlessHeapIndex(inData.mProbesDepthTextureSRV),
                .mProbesIrradianceTexture = inResources.GetBindlessHeapIndex(inData.mProbesIrradianceTextureSRV)
            },
            .mOutputTexture = inResources.GetBindlessHeapIndex(inData.mOutputTexture),
            .mDepthTexture = inResources.GetBindlessHeapIndex(inData.mDepthTextureSRV),
            .mGBufferTexture = inResources.GetBindlessHeapIndex(inData.mGBufferTextureSRV),
            .mDispatchSize = UVec2(output_texture.GetWidth(), output_texture.GetHeight())
        };

        inCmdList->SetPipelineState(g_SystemShaders.mProbeSampleShader.GetComputePSO());
        inCmdList.PushComputeConstants(root_constants);

        inCmdList->Dispatch(( output_texture.GetWidth() + 7 ) / 8, ( output_texture.GetHeight() + 7 ) / 8, 1);
    });

    return DDGIOutput
    {
        .mOutput = sample_data.mOutputTexture,
        .mDepthProbes = trace_data.mProbesDepthTexture,
        .mIrradianceProbes = trace_data.mProbesIrradianceTexture,
        .mProbeData = trace_data.mProbeDataBuffer
    };
}



const ProbeDebugData& AddProbeDebugPass(RenderGraph& inRenderGraph, Device& inDevice, const RK::Mesh& inProbeMesh, const DDGIOutput& inDDGI, RenderGraphResourceID inRenderTarget, RenderGraphResourceID inDepthTarget)
{
    return inRenderGraph.AddGraphicsPass<ProbeDebugData>("DDGI Debug Probes",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ProbeDebugData& inData)
    {
        inData.mProbeMesh = &inProbeMesh;

        inData.mRenderTargetRTV = ioRGBuilder.RenderTarget(inRenderTarget);
        inData.mDepthTargetDSV = ioRGBuilder.DepthStencilTarget(inDepthTarget);

        inData.mProbeDataBufferSRV = ioRGBuilder.Read(inDDGI.mProbeData);
        inData.mProbesDepthTextureSRV = ioRGBuilder.Read(inDDGI.mDepthProbes);
        inData.mProbesIrradianceTextureSRV = ioRGBuilder.Read(inDDGI.mIrradianceProbes);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = inRenderPass->CreatePipelineStateDesc(inDevice, g_SystemShaders.mProbeDebugShader);

        constexpr std::array vertex_layout =
        {
            D3D12_INPUT_ELEMENT_DESC { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, pos),     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            D3D12_INPUT_ELEMENT_DESC { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, uv),      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            D3D12_INPUT_ELEMENT_DESC { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            D3D12_INPUT_ELEMENT_DESC { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, tangent), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        pso_desc.InputLayout.NumElements = vertex_layout.size();
        pso_desc.InputLayout.pInputElementDescs = vertex_layout.data();
        
        inData.mPipeline = inDevice.CreateGraphicsPipeline(pso_desc);
        inData.mPipeline->SetName(L"PSO_PROBE_DEBUG");
    },

    [&inDevice](ProbeDebugData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.SetViewportAndScissor(inDevice.GetTexture(inResources.GetTextureView(inData.mRenderTargetRTV)));

        inCmdList.PushGraphicsConstants(DDGIData {
            .mProbeCount = RenderSettings::mDDGIProbeCount,
            .mProbeRadius = RenderSettings::mDDGIDebugRadius,
            .mProbeSpacing = RenderSettings::mDDGIProbeSpacing,
            .mCornerPosition = RenderSettings::mDDGICornerPosition,
            .mProbesDataBuffer = inResources.GetBindlessHeapIndex(inData.mProbeDataBufferSRV),
            .mProbesDepthTexture = inResources.GetBindlessHeapIndex(inData.mProbesDepthTextureSRV),
            .mProbesIrradianceTexture = inResources.GetBindlessHeapIndex(inData.mProbesIrradianceTextureSRV)
        });

        inCmdList.BindVertexAndIndexBuffers(inDevice, *inData.mProbeMesh);

        const int total_probe_count = RenderSettings::mDDGIProbeCount.x * RenderSettings::mDDGIProbeCount.y * RenderSettings::mDDGIProbeCount.z;
        inCmdList->DrawIndexedInstanced(inData.mProbeMesh->indices.size(), total_probe_count, 0, 0, 0);
    });
}



const ProbeDebugRaysData& AddProbeDebugRaysPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inRenderTarget, RenderGraphResourceID inDepthTarget, BufferID inLinesVertexBuffer, BufferID inIndirectArgsBuffer)
{
    return inRenderGraph.AddGraphicsPass<ProbeDebugRaysData>("DDGI Debug Rays",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ProbeDebugRaysData& inData)
    {
        inData.mIndirectArgsBuffer = ioRGBuilder.Import(inDevice, inIndirectArgsBuffer);
        ioRGBuilder.ReadIndirectArgs(inData.mIndirectArgsBuffer);

        ioRGBuilder.RenderTarget(inRenderTarget);
        ioRGBuilder.DepthStencilTarget(inDepthTarget);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inRenderPass->CreatePipelineStateDesc(inDevice, g_SystemShaders.mProbeDebugRaysShader);

        pso_state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso_state.RasterizerState.AntialiasedLineEnable = true;
        pso_state.InputLayout = {};

        inData.mPipeline = inDevice.CreateGraphicsPipeline(pso_state);
        inData.mPipeline->SetName(L"PSO_PROBE_DEBUG_RAYS");
    },

    [&inDevice](ProbeDebugRaysData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
        inCmdList->SetPipelineState(inData.mPipeline.Get());

        ID3D12Resource* indirect_args_buffer_resource = inDevice.GetD3D12Resource(inRGResources.GetBuffer(inData.mIndirectArgsBuffer));
        inCmdList->ExecuteIndirect(inDevice.GetCommandSignature(COMMAND_SIGNATURE_DRAW), 1, indirect_args_buffer_resource, 0, nullptr, 0);
        
        inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    });
}

} // Raekor
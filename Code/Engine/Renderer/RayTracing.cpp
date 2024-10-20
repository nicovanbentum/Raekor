#include "PCH.h"
#include "RayTracing.h"

#include "Scene.h"
#include "Shader.h"
#include "RenderPasses.h"

#include "Camera.h"
#include "Primitives.h"

namespace RK::DX12 {

RTTI_DEFINE_TYPE(ClearShadowsData)        {}
RTTI_DEFINE_TYPE(ClearShadowTilesData)    {}
RTTI_DEFINE_TYPE(TraceShadowTilesData)    {}
RTTI_DEFINE_TYPE(ClassifyShadowTilesData) {}
RTTI_DEFINE_TYPE(DenoiseShadowsData)      {}
RTTI_DEFINE_TYPE(RTAOData)                {}
RTTI_DEFINE_TYPE(ReflectionsData)         {}
RTTI_DEFINE_TYPE(ProbeTraceData)          {}
RTTI_DEFINE_TYPE(ProbeUpdateData)         {}
RTTI_DEFINE_TYPE(ProbeSampleData)         {}
RTTI_DEFINE_TYPE(PathTraceData)           {}
RTTI_DEFINE_TYPE(ProbeDebugData)          {}
RTTI_DEFINE_TYPE(ProbeDebugRaysData)      {}


void ClearTextureUAV(Device& inDevice, TextureID inTexture, Vec4 inValue, CommandList& inCmdList)
{
    inCmdList.PushComputeConstants(ClearTextureRootConstants
    {
        .mClearValue = inValue,
        .mTexture = inDevice.GetBindlessHeapIndex(inTexture)
    });

    const Texture& texture = inDevice.GetTexture(inTexture);

    inCmdList->SetPipelineState(g_SystemShaders.mClearTextureShader.GetComputePSO());
    inCmdList->Dispatch(( texture.GetWidth() + 7 ) / 8, ( texture.GetHeight() + 7 ) / 8, 1);
}


const RenderGraphResourceID AddRayTracedShadowsPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferData& inGBufferData)
{
    static constexpr int cTileSize = RT_SHADOWS_GROUP_DIM;
    static constexpr int cPackedRaysWidth = RT_SHADOWS_PACKED_DIM_X;
    static constexpr int cPackedRaysHeight = RT_SHADOWS_PACKED_DIM_Y;

    auto TraceShadowRaysPass = [](RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferData& inGBufferData)
    {
        return inRenderGraph.AddComputePass<TraceShadowTilesData>("TRACE SHADOW RAYS PASS",
        [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, TraceShadowTilesData& inData)
        {
            inData.mOutputTexture = inRGBuilder.Create(Texture::Desc
            {
                .format = DXGI_FORMAT_R32_UINT,
                .width  = (inRenderGraph.GetViewport().size.x + cPackedRaysWidth - 1 ) / cPackedRaysWidth,
                .height = (inRenderGraph.GetViewport().size.y + cPackedRaysHeight  - 1 ) / cPackedRaysHeight,
                .usage  = Texture::Usage::SHADER_READ_WRITE,
                .debugName = "RT_PackedShadowRayHits"
            });

            inRGBuilder.Write(inData.mOutputTexture);
            inData.mGBufferDepthTextureSRV = inRGBuilder.Read(inGBufferData.mDepthTexture);
            inData.mGBufferRenderTextureSRV = inRGBuilder.Read(inGBufferData.mRenderTexture);
        },

        [&inRenderGraph, &inDevice, &inScene](TraceShadowTilesData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
        {
            if (inScene->Count<Mesh>() == 0)
                return;

            const Viewport& viewport = inRenderGraph.GetViewport();
            const Texture& texture  = inDevice.GetTexture(inResources.GetTexture(inData.mOutputTexture));

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

    auto ClearShadowTilesPass = [](RenderGraph& inRenderGraph, Device& inDevice)
    {
        return inRenderGraph.AddComputePass<ClearShadowTilesData>("CLEAR SHADOW TILES PASS",
        [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, ClearShadowTilesData& inData)
        {
            const UVec2& render_size = inRenderGraph.GetViewport().GetRenderSize();
            const uint32_t tile_width = ( render_size.x + cTileSize - 1 ) / cTileSize;
            const uint32_t tile_height = ( render_size.y + cTileSize - 1 ) / cTileSize;
            const uint32_t tile_buffer_size = tile_width * tile_height * sizeof(uint32_t);

            inData.mTilesBuffer = inRGBuilder.Create(Buffer::RWTypedBuffer(DXGI_FORMAT_R32_UINT, tile_buffer_size, "ShadowTilesBuffer"));
            inData.mIndirectDispatchBuffer = inRGBuilder.Create(Buffer::RWByteAddressBuffer(sizeof(D3D12_DISPATCH_ARGUMENTS), "ShadowsDispatchBuffer"));

            inRGBuilder.Write(inData.mTilesBuffer);
            inRGBuilder.Write(inData.mIndirectDispatchBuffer);
        },

        [&inRenderGraph, &inDevice](ClearShadowTilesData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
        {
            inCmdList.PushComputeConstants(ShadowsClearRootConstants
            {
                .mTilesBuffer = inDevice.GetBindlessHeapIndex(inResources.GetBuffer(inData.mTilesBuffer)),
                .mDispatchBuffer = inDevice.GetBindlessHeapIndex(inResources.GetBuffer(inData.mIndirectDispatchBuffer))
            });

            const Buffer& tiles_buffer = inDevice.GetBuffer(inResources.GetBuffer(inData.mTilesBuffer));
            
            inCmdList->SetPipelineState(g_SystemShaders.mClearShadowTilesShader.GetComputePSO());
            inCmdList->Dispatch(( (tiles_buffer.GetSize() / sizeof(uint32_t)) + 63 ) / 64, 1, 1);
        });
    };

    auto ClassifyShadowTilesPass = [](RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const TraceShadowTilesData& inTraceData, const ClearShadowTilesData& inClearData)
    {
        return inRenderGraph.AddComputePass<ClassifyShadowTilesData>("CLASSIFY SHADOW RAYS PASS",
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
            const UVec2 dispatch_size = UVec2(( viewport.size.x + cTileSize - 1 ) / cTileSize, ( viewport.size.y + cTileSize - 1 ) / cTileSize);

            inCmdList.PushComputeConstants(ShadowsClassifyRootConstants 
            { 
                .mShadowMaskTexture = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mTracedShadowRaysTextureSRV)),
                .mTilesBuffer       = inDevice.GetBindlessHeapIndex(inResources.GetBufferView(inData.mTilesBufferUAV)),
                .mDispatchBuffer    = inDevice.GetBindlessHeapIndex(inResources.GetBufferView(inData.mIndirectDispatchBufferUAV)),
                .mDispatchSize      = viewport.size
            });

            inCmdList->SetPipelineState(g_SystemShaders.mClassifyShadowTilesShader.GetComputePSO());
            inCmdList->Dispatch(dispatch_size.x, dispatch_size.y, 1);
        });
    };

    auto ClearShadowsPass = [](RenderGraph& inRenderGraph, Device& inDevice)
    {
        return inRenderGraph.AddComputePass<ClearShadowsData>("CLEAR SHADOWS PASS",
        [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, ClearShadowsData& inData)
        {
            inData.mShadowsTexture = inRGBuilder.Create(Texture::Desc
            {
                .format = DXGI_FORMAT_R32G32_FLOAT,
                .width  = inRenderGraph.GetViewport().size.x,
                .height = inRenderGraph.GetViewport().size.y,
                .usage  = Texture::Usage::SHADER_READ_WRITE,
                .debugName = "RT_ShadowMask"
            });

            inData.mShadowsTextureHistory = inRGBuilder.Create(Texture::Desc
            {
                .format = DXGI_FORMAT_R32G32_FLOAT,
                .width  = inRenderGraph.GetViewport().size.x,
                .height = inRenderGraph.GetViewport().size.y,
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

    auto DenoiseShadowsPass = [](RenderGraph& inRenderGraph, Device& inDevice, const GBufferData& inGBufferData, const TraceShadowTilesData& inTraceData, const ClearShadowTilesData& inTilesData, const ClearShadowsData& inShadowsData)
    {
        return inRenderGraph.AddComputePass<DenoiseShadowsData>("DENOISE SHADOWS PASS",
        [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, DenoiseShadowsData& inData)
        {
            inData.mOutputTextureUAV = inRGBuilder.Write(inShadowsData.mShadowsTexture);
            inData.mHistoryTextureUAV = inRGBuilder.Write(inShadowsData.mShadowsTextureHistory);

            inData.mDepthTextureSRV = inRGBuilder.Read(inGBufferData.mDepthTexture);
            inData.mGBufferTextureSRV = inRGBuilder.Read(inGBufferData.mRenderTexture);
            inData.mVelocityTextureSRV = inRGBuilder.Read(inGBufferData.mVelocityTexture);
            inData.mSelectionTextureSRV = inRGBuilder.Read(inGBufferData.mSelectionTexture);

            inData.mTracedShadowRaysTextureSRV = inRGBuilder.Read(inTraceData.mOutputTexture);

            inData.mTilesBufferSRV = inRGBuilder.Read(inTilesData.mTilesBuffer);
            inData.mDenoisedTilesBufferSRV = inRGBuilder.Read(inTilesData.mTilesBuffer);

            // don't actually need the views, just barriers and render graph resource IDs
            inData.mIndirectDispatchBufferSRV = inRGBuilder.ReadIndirectArgs(inTilesData.mIndirectDispatchBuffer);
            inData.mDenoisedIndirectDispatchBufferSRV = inRGBuilder.ReadIndirectArgs(inTilesData.mIndirectDispatchBuffer);
        },

        [&inRenderGraph, &inDevice](DenoiseShadowsData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
        {
            ShadowsDenoiseRootConstants root_constants =
            {
                .mResultTexture     = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mOutputTextureUAV)),
                .mHistoryTexture    = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mHistoryTextureUAV)),
                .mDepthTexture      = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mDepthTextureSRV)),
                .mVelocityTexture   = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mVelocityTextureSRV)),
                .mShadowMaskTexture = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mTracedShadowRaysTextureSRV)),
                .mSelectionTexture  = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mSelectionTextureSRV)),
                .mDispatchSize      = inRenderGraph.GetViewport().GetRenderSize()
            };

            //{
            //    // write black to the final shadow texture for all the tiles that didn't need denoising
            //    Buffer& dispatch_buffer = inResources.GetBuffer(inData.mIndirectDispatchBufferSRV);
            //    inCmdList->ExecuteIndirect(inDevice.GetCommandSignature(COMMAND_SIGNATURE_DISPATCH), 1, inDevice.GetD3D12Resource(dispatch_buffer), 0, nullptr, 0);
            //}

            {
                // write all the denoised tiles to the final shadow texture
                root_constants.mTilesBuffer = inDevice.GetBindlessHeapIndex(inResources.GetBufferView(inData.mDenoisedTilesBufferSRV));
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

    const TraceShadowTilesData& traced_rays_data = TraceShadowRaysPass(inRenderGraph, inDevice, inScene, inGBufferData);

    const ClearShadowTilesData& clear_tiles_data = ClearShadowTilesPass(inRenderGraph, inDevice);

    const ClassifyShadowTilesData& classify_data = ClassifyShadowTilesPass(inRenderGraph, inDevice, inScene, traced_rays_data, clear_tiles_data);

    const ClearShadowsData& clear_shadows_data   = ClearShadowsPass(inRenderGraph, inDevice);

    const DenoiseShadowsData& denoise_tiles_data = DenoiseShadowsPass(inRenderGraph, inDevice, inGBufferData, traced_rays_data, clear_tiles_data, clear_shadows_data);

    return clear_shadows_data.mShadowsTexture;
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
            .debugName = "RT_AOMask"
        });

        inRGBuilder.Write(inData.mOutputTexture);

        inData.mGbufferDepthTextureSRV = inRGBuilder.Read(inGbufferData.mDepthTexture);
        inData.mGBufferRenderTextureSRV = inRGBuilder.Read(inGbufferData.mRenderTexture);
    },

    [&inRenderGraph, &inDevice, &inScene](RTAOData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        if (inScene->Count<Mesh>() == 0)
            return;

        const Viewport& viewport = inRenderGraph.GetViewport();

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



const ReflectionsData& AddReflectionsPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferData& inGBufferData, const SkyCubeData& inSkyCubeData)
{
    return inRenderGraph.AddComputePass<ReflectionsData>("RAY TRACED REFLECTIONS PASS",
        [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, ReflectionsData& inData)
    {
        inData.mOutputTexture = inRGBuilder.Create(Texture::Desc
            {
                .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
                .width = inRenderGraph.GetViewport().size.x,
                .height = inRenderGraph.GetViewport().size.y,
                .mipLevels = 0, // let it calculate the nr of mips
                .usage = Texture::Usage::SHADER_READ_WRITE,
                .debugName = "RT_Reflections"
            });

        inRGBuilder.Write(inData.mOutputTexture);

        inData.mSkyCubeTextureSRV = inRGBuilder.Read(inSkyCubeData.mSkyCubeTexture);
        inData.mGBufferDepthTextureSRV = inRGBuilder.Read(inGBufferData.mDepthTexture);
        inData.mGbufferRenderTextureSRV = inRGBuilder.Read(inGBufferData.mRenderTexture);
    },

    [&inRenderGraph, &inDevice, &inScene](ReflectionsData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        if (inScene->Count<Mesh>() == 0)
            return;

        const Viewport& viewport = inRenderGraph.GetViewport();

        inCmdList.PushComputeConstants(ReflectionsRootConstants {
            .mTLAS = inDevice.GetBindlessHeapIndex(inScene.GetTLASDescriptor(inDevice)),
            .mInstancesBuffer = inDevice.GetBindlessHeapIndex(inScene.GetInstancesDescriptor(inDevice)),
            .mMaterialsBuffer = inDevice.GetBindlessHeapIndex(inScene.GetMaterialsDescriptor(inDevice)),
            .mResultTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mOutputTexture)),
            .mSkyCubeTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mSkyCubeTextureSRV)),
            .mGbufferDepthTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mGBufferDepthTextureSRV)),
            .mGbufferRenderTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mGbufferRenderTextureSRV)),
            .mDispatchSize = viewport.size
            });

        inCmdList->SetPipelineState(g_SystemShaders.mRTReflectionsShader.GetComputePSO());
        inCmdList->Dispatch(( viewport.size.x + 7 ) / 8, ( viewport.size.y + 7 ) / 8, 1);
    });
}



const PathTraceData& AddPathTracePass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const SkyCubeData& inSkyCubeData)
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
            .debugName = "RT_PathTraceOutput"
        });

        inData.mAccumulationTexture = inRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = "RT_PathTraceAccumulation"
        });

        inRGBuilder.Write(inData.mOutputTexture);
        inRGBuilder.Write(inData.mAccumulationTexture);

        inData.mSkyCubeTextureSRV = inRGBuilder.Read(inSkyCubeData.mSkyCubeTexture);
    },

    [&inRenderGraph, &inDevice, &inScene](PathTraceData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        const Viewport& viewport = inRenderGraph.GetViewport();

        PathTraceRootConstants constants =
        {
            .mTLAS = inDevice.GetBindlessHeapIndex(inScene.GetTLASDescriptor(inDevice)),
            .mBounces = inData.mBounces,
            .mInstancesBuffer = inDevice.GetBindlessHeapIndex(inScene.GetInstancesDescriptor(inDevice)),
            .mMaterialsBuffer = inDevice.GetBindlessHeapIndex(inScene.GetMaterialsDescriptor(inDevice)),
            .mResultTexture = inDevice.GetBindlessHeapIndex(inResources.GetTexture(inData.mOutputTexture)),
            .mAccumulationTexture = inDevice.GetBindlessHeapIndex(inResources.GetTexture(inData.mAccumulationTexture)),
            .mSkyCubeTexture = inDevice.GetBindlessHeapIndex(inResources.GetTextureView(inData.mSkyCubeTextureSRV)),
            .mDispatchSize = viewport.size,
            .mReset = PathTraceData::mReset,
        };

        constants.mLightsCount = inScene->Count<Light>();

        if (constants.mLightsCount > 0)
            constants.mLightsBuffer = inDevice.GetBindlessHeapIndex(inScene.GetLightsDescriptor(inDevice));

        inCmdList.PushComputeConstants(constants);

        inCmdList->SetPipelineState(g_SystemShaders.mRTPathTraceShader.GetComputePSO());
        inCmdList->Dispatch(( viewport.size.x + 7 ) / 8, ( viewport.size.y + 7 ) / 8, 1);

        PathTraceData::mReset = false;
    });
}



const ProbeTraceData& AddProbeTracePass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const SkyCubeData& inSkyCubeData)
{
    return inRenderGraph.AddComputePass<ProbeTraceData>("GI PROBE TRACE PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ProbeTraceData& inData)
    {
        const int total_probe_count = inData.mDDGIData.mProbeCount.x * inData.mDDGIData.mProbeCount.y * inData.mDDGIData.mProbeCount.z;

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

        ioRGBuilder.Write(inData.mRaysDepthTexture);
        ioRGBuilder.Write(inData.mRaysIrradianceTexture);

        inData.mSkyCubeTextureSRV = ioRGBuilder.Read(inSkyCubeData.mSkyCubeTexture);
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
            const Entity& ddgi_entity = inScene->GetEntities<DDGISceneSettings>()[0];
            const Transform& ddgi_transform = inScene->Get<Transform>(ddgi_entity);
            const DDGISceneSettings& ddgi_settings = inScene->Get<DDGISceneSettings>(ddgi_entity);

            inData.mDDGIData.mProbeCount = ddgi_settings.mDDGIProbeCount;
            inData.mDDGIData.mProbeSpacing = ddgi_settings.mDDGIProbeSpacing;
            inData.mDDGIData.mCornerPosition = ddgi_transform.position;
        }

        inData.mRandomRotationMatrix = gRandomOrientation();
        inData.mDDGIData.mRaysDepthTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mRaysDepthTexture));
        inData.mDDGIData.mRaysIrradianceTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mRaysIrradianceTexture));

        ProbeTraceRootConstants root_constants =
        {
            .mInstancesBuffer = inDevice.GetBindlessHeapIndex(inScene.GetInstancesDescriptor(inDevice)),
            .mTLAS = inDevice.GetBindlessHeapIndex(inScene.GetTLASDescriptor(inDevice)),
            .mDebugProbeIndex = Index3Dto1D(inData.mDebugProbe, inData.mDDGIData.mProbeCount),
            .mSkyCubeTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mSkyCubeTextureSRV)),
            .mRandomRotationMatrix = inData.mRandomRotationMatrix,
            .mDDGIData = inData.mDDGIData
        };

        if (inScene->Count<Material>())
            root_constants.mMaterialsBuffer = inDevice.GetBindlessHeapIndex(inScene.GetMaterialsDescriptor(inDevice));
        
        if (root_constants.mLightsCount = inScene->Count<Light>())
            root_constants.mLightsBuffer = inDevice.GetBindlessHeapIndex(inScene.GetLightsDescriptor(inDevice));

        inCmdList.PushComputeConstants(root_constants);

        const int total_probe_count = inData.mDDGIData.mProbeCount.x * inData.mDDGIData.mProbeCount.y * inData.mDDGIData.mProbeCount.z;

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
        const int total_probe_count = inData.mDDGIData.mProbeCount.x * inData.mDDGIData.mProbeCount.y * inData.mDDGIData.mProbeCount.z;

        inData.mProbesDepthTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16_FLOAT,
            .width  = uint32_t(DDGI_DEPTH_TEXELS * DDGI_PROBES_PER_ROW),
            .height = uint32_t(DDGI_DEPTH_TEXELS * ( total_probe_count / DDGI_PROBES_PER_ROW )),
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = "DDGI_UpdatedDepth"
        });

        inData.mProbesIrradianceTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = uint32_t(DDGI_IRRADIANCE_TEXELS * DDGI_PROBES_PER_ROW),
            .height = uint32_t(DDGI_IRRADIANCE_TEXELS * ( total_probe_count / DDGI_PROBES_PER_ROW )),
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = "DDGI_UpdatedIrradiance"
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

        inData.mDDGIData = inTraceData.mDDGIData;
        inData.mDDGIData.mRaysDepthTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mRaysDepthTextureSRV));
        inData.mDDGIData.mProbesDepthTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mProbesDepthTexture));
        inData.mDDGIData.mRaysIrradianceTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mRaysIrradianceTextureSRV));
        inData.mDDGIData.mProbesIrradianceTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mProbesIrradianceTexture));

        /*{
            inCmdList->SetPipelineState(g_SystemShaders.mProbeUpdateDepthShader.GetComputePSO());
            inCmdList.PushComputeConstants(ProbeUpdateRootConstants
            {
                .mRandomRotationMatrix = inTraceData.mRandomRotationMatrix,
                .mDDGIData = inData.mDDGIData
            });

            const Texture& depth_texture = inDevice.GetTexture(inRGResources.GetTexture(inData.mProbesDepthTexture));
            inCmdList->Dispatch(depth_texture.GetDesc().width / DDGI_DEPTH_TEXELS, depth_texture.GetDesc().height / DDGI_DEPTH_TEXELS, 1);
        }*/

        if (ProbeUpdateData::mClear)
        {
            ClearTextureUAV(inDevice, inRGResources.GetTexture(inData.mProbesIrradianceTexture), Vec4(0.0), inCmdList);
            ProbeUpdateData::mClear = false;
        }

        {
            inCmdList->SetPipelineState(g_SystemShaders.mProbeUpdateIrradianceShader.GetComputePSO());
            inCmdList.PushComputeConstants(ProbeUpdateRootConstants
            {
                .mRandomRotationMatrix = inTraceData.mRandomRotationMatrix,
                .mDDGIData = inData.mDDGIData
            });

            const Texture& irradiance_texture = inDevice.GetTexture(inRGResources.GetTexture(inData.mProbesIrradianceTexture));
            inCmdList->Dispatch(irradiance_texture.GetWidth() / DDGI_IRRADIANCE_TEXELS, irradiance_texture.GetHeight() / DDGI_IRRADIANCE_TEXELS, 1);
        }
    });
}



const ProbeSampleData& AddProbeSamplePass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferData& inGBufferData, const ProbeUpdateData& inProbeData)
{
    return inRenderGraph.AddComputePass<ProbeSampleData>("GI PROBE SAMPLE PASS",
        [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ProbeSampleData& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().size.x,
            .height = inRenderGraph.GetViewport().size.y,
            .usage  = Texture::SHADER_READ_WRITE,
            .debugName = "RT_DDGISampleOutput"
        });

        ioRGBuilder.Write(inData.mOutputTexture);

        inData.mDDGIData = inProbeData.mDDGIData;
        inData.mDepthTextureSRV = ioRGBuilder.Read(inGBufferData.mDepthTexture);
        inData.mGBufferTextureSRV = ioRGBuilder.Read(inGBufferData.mRenderTexture);
        inData.mProbesDepthTextureSRV = ioRGBuilder.Read(inProbeData.mProbesDepthTexture);
        inData.mProbesIrradianceTextureSRV = ioRGBuilder.Read(inProbeData.mProbesIrradianceTexture);
    },

        [&inRenderGraph, &inDevice, &inProbeData](ProbeSampleData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        const Viewport& viewport = inRenderGraph.GetViewport();

        inData.mDDGIData = inProbeData.mDDGIData;
        inData.mDDGIData.mProbesDepthTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mProbesDepthTextureSRV));
        inData.mDDGIData.mProbesIrradianceTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mProbesIrradianceTextureSRV));

        inCmdList->SetPipelineState(g_SystemShaders.mProbeSampleShader.GetComputePSO());
        inCmdList.PushComputeConstants(ProbeSampleRootConstants
            {
                .mDDGIData = inData.mDDGIData,
                .mOutputTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mOutputTexture)),
                .mDepthTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mDepthTextureSRV)),
                .mGBufferTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mGBufferTextureSRV)),
                .mDispatchSize = viewport.GetRenderSize()
            });

        inCmdList->Dispatch(( viewport.GetRenderSize().x + 7 ) / 8, ( viewport.GetRenderSize().y + 7 ) / 8, 1);
    });
}



const ProbeDebugData& AddProbeDebugPass(RenderGraph& inRenderGraph, Device& inDevice, const ProbeTraceData& inTraceData, const ProbeUpdateData& inUpdateData, RenderGraphResourceID inRenderTarget, RenderGraphResourceID inDepthTarget)
{
    return inRenderGraph.AddGraphicsPass<ProbeDebugData>("GI PROBE DEBUG PASS",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ProbeDebugData& inData)
    {
        Mesh::CreateSphere(inData.mProbeMesh, 0.5f, 32u, 32u);

        const uint64_t indices_size = inData.mProbeMesh.indices.size() * sizeof(inData.mProbeMesh.indices[0]);
        const uint64_t vertices_size = inData.mProbeMesh.vertices.size() * sizeof(inData.mProbeMesh.vertices[0]);

        inData.mProbeMesh.indexBuffer = inDevice.CreateBuffer(Buffer::Desc
        {
            .size   = uint32_t(indices_size),
            .stride = sizeof(uint32_t) * 3,
            .usage  = Buffer::Usage::INDEX_BUFFER,
            .mappable = true,
            .debugName = "DebugProbeIndices"
        }).GetValue();

        inData.mProbeMesh.vertexBuffer = inDevice.CreateBuffer(Buffer::Desc
        {
            .size   = uint32_t(vertices_size),
            .stride = sizeof(Vertex),
            .usage  = Buffer::Usage::VERTEX_BUFFER,
            .mappable = true,
            .debugName = "DebugProbeVertices"
        }).GetValue();

        {
            Buffer& index_buffer = inDevice.GetBuffer(BufferID(inData.mProbeMesh.indexBuffer));
            void* mapped_ptr = nullptr;
            index_buffer->Map(0, nullptr, &mapped_ptr);
            memcpy(mapped_ptr, inData.mProbeMesh.indices.data(), indices_size);
            index_buffer->Unmap(0, nullptr);
        }

        {
            Buffer& vertex_buffer = inDevice.GetBuffer(BufferID(inData.mProbeMesh.vertexBuffer));
            void* mapped_ptr = nullptr;
            vertex_buffer->Map(0, nullptr, &mapped_ptr);
            memcpy(mapped_ptr, inData.mProbeMesh.vertices.data(), vertices_size);
            vertex_buffer->Unmap(0, nullptr);
        }

        inData.mDDGIData = inUpdateData.mDDGIData;

        inData.mRenderTargetRTV = ioRGBuilder.RenderTarget(inRenderTarget);
        inData.mDepthTargetDSV = ioRGBuilder.DepthStencilTarget(inDepthTarget);

        inData.mProbesDepthTextureSRV = ioRGBuilder.Read(inUpdateData.mProbesDepthTexture);
        inData.mProbesIrradianceTextureSRV = ioRGBuilder.Read(inUpdateData.mProbesIrradianceTexture);

        ByteSlice vertex_shader, pixel_shader;
        g_SystemShaders.mProbeDebugShader.GetGraphicsProgram(vertex_shader, pixel_shader);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = inDevice.CreatePipelineStateDesc(inRenderPass, vertex_shader, pixel_shader);

        constexpr std::array vertex_layout =
        {
            D3D12_INPUT_ELEMENT_DESC { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, pos),     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            D3D12_INPUT_ELEMENT_DESC { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, uv),      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            D3D12_INPUT_ELEMENT_DESC { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            D3D12_INPUT_ELEMENT_DESC { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, tangent), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        pso_desc.InputLayout.NumElements = vertex_layout.size();
        pso_desc.InputLayout.pInputElementDescs = vertex_layout.data();
        
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

        const int total_probe_count = inData.mDDGIData.mProbeCount.x * inData.mDDGIData.mProbeCount.y * inData.mDDGIData.mProbeCount.z;
        inCmdList->DrawIndexedInstanced(inData.mProbeMesh.indices.size(), total_probe_count, 0, 0, 0);
    });
}



const ProbeDebugRaysData& AddProbeDebugRaysPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inRenderTarget, RenderGraphResourceID inDepthTarget)
{
    return inRenderGraph.AddGraphicsPass<ProbeDebugRaysData>("GI PROBE DEBUG RAYS PASS",

        [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ProbeDebugRaysData& inData)
    {
        inData.mVertexBuffer = ioRGBuilder.Create(Buffer::Desc
        {
            .size   = sizeof(Vec4) * UINT16_MAX,
            .stride = sizeof(Vec4),
            .usage  = Buffer::Usage::SHADER_READ_WRITE,
            .debugName = "DebugLinesVertexBuffer"
        });


        inData.mIndirectArgsBuffer = ioRGBuilder.Create(Buffer::Desc
        {
            .size  = sizeof(D3D12_DRAW_ARGUMENTS),
            .usage = Buffer::Usage::SHADER_READ_WRITE,
            .debugName = "DebugLinesIndirectArgsBuffer"
        });

        // ioRGBuilder.Write(inData.mVertexBuffer);

        const RenderGraphResourceViewID render_target = ioRGBuilder.RenderTarget(inRenderTarget);
        const RenderGraphResourceViewID depth_target = ioRGBuilder.DepthStencilTarget(inDepthTarget);

        D3D12_INDIRECT_ARGUMENT_DESC indirect_args =
        {
            .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW
        };

        const D3D12_COMMAND_SIGNATURE_DESC cmdsig_desc =
        {
            .ByteStride = sizeof(D3D12_DRAW_ARGUMENTS),
            .NumArgumentDescs = 1,
            .pArgumentDescs = &indirect_args
        };

        inDevice->CreateCommandSignature(&cmdsig_desc, nullptr, IID_PPV_ARGS(inData.mCommandSignature.GetAddressOf()));

        constexpr std::array vertex_layout =
        {
            D3D12_INPUT_ELEMENT_DESC { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        ByteSlice vertex_shader, pixel_shader;
        g_SystemShaders.mProbeDebugRaysShader.GetGraphicsProgram(vertex_shader, pixel_shader);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inDevice.CreatePipelineStateDesc(inRenderPass, vertex_shader, pixel_shader);

        pso_state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso_state.RasterizerState.AntialiasedLineEnable = true;
        pso_state.InputLayout.NumElements = uint32_t(vertex_layout.size());
        pso_state.InputLayout.pInputElementDescs = vertex_layout.data();

        inDevice->CreateGraphicsPipelineState(&pso_state, IID_PPV_ARGS(inData.mPipeline.GetAddressOf()));
        inData.mPipeline->SetName(L"PSO_PROBE_DEBUG_RAYS");
    },

        [&inDevice](ProbeDebugRaysData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        ID3D12Resource* vertices_buffer_resource = inDevice.GetD3D12Resource(inRGResources.GetBuffer(inData.mVertexBuffer));
        ID3D12Resource* indirect_args_buffer_resource = inDevice.GetD3D12Resource(inRGResources.GetBuffer(inData.mIndirectArgsBuffer));

        const auto vertices_entry_barrier = CD3DX12_RESOURCE_BARRIER::Transition(vertices_buffer_resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        const auto indirect_args_entry_barrier = CD3DX12_RESOURCE_BARRIER::Transition(indirect_args_buffer_resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        const std::array entry_barriers = { vertices_entry_barrier, indirect_args_entry_barrier };

        const auto vertices_exit_barrier = CD3DX12_RESOURCE_BARRIER::Transition(vertices_buffer_resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        const auto indirect_args_exit_barrier = CD3DX12_RESOURCE_BARRIER::Transition(indirect_args_buffer_resource, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        const std::array exit_barriers = { vertices_exit_barrier, indirect_args_exit_barrier };

        const Buffer& vertex_buffer = inDevice.GetBuffer(inRGResources.GetBuffer(inData.mVertexBuffer));

        const D3D12_VERTEX_BUFFER_VIEW vertex_view = 
        {
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

} // Raekor
#include "PCH.h"
#include "RayTracing.h"

#include "Scene.h"
#include "Shader.h"
#include "GPUProfiler.h"
#include "RenderPasses.h"

#include "Camera.h"
#include "Primitives.h"

namespace RK::DX12 {

const BuildAccelerationStructuresData& AddBuildAccelerationStructuresPass(RenderGraph& inRenderGraph, Device& inDevice, const RenderWorld& inWorld, GPUScene& inGPUScene)
{
    return inRenderGraph.AddComputePass<BuildAccelerationStructuresData>("Build Acceleration Structures",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, BuildAccelerationStructuresData& inData)
    {
    },
    [&inDevice, &inWorld, &inGPUScene](BuildAccelerationStructuresData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        static const int& update_skinning = g_CVariables->Create("update_skinning", 1, true);

        const D3D12_RESOURCE_BARRIER uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);

        if (update_skinning)
        {
            for (const RenderSkinnedMesh& skinned_mesh : inWorld.GetSkinnedMeshes())
                inGPUScene.RefitBottomLevelAS(inDevice, inCmdList, skinned_mesh);
        }

        inCmdList->ResourceBarrier(1, &uav_barrier);

        inGPUScene.BuildTLAS(inDevice, inCmdList);

        inCmdList->ResourceBarrier(1, &uav_barrier);
    });
}



const RenderGraphResourceID AddRayTracedShadowsPass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferOutput& inGBuffer)
{
    const TraceShadowsData& trace_data = inRenderGraph.AddComputePass<TraceShadowsData>("RT Shadows Trace",
    [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, TraceShadowsData& inData)
    {
        inData.mOutputTexture = inRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R8_UNORM,
            .width  = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = "RT_ShadowRays"
        });

        inRGBuilder.Write(inData.mOutputTexture);
        inData.mGBufferDepthTextureSRV = inRGBuilder.Read(inGBuffer.mDepthTexture);
        inData.mGBufferRenderTextureSRV = inRGBuilder.Read(inGBuffer.mRenderTexture);
    },

    [&inRenderGraph, &inDevice](TraceShadowsData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        const Viewport& viewport = inRenderGraph.GetViewport();

        inCmdList.PushComputeConstants(ShadowMaskRootConstants
        {
            .mShadowMaskTexture = inResources.GetBindlessHeapIndex(inData.mOutputTexture),
            .mGbufferDepthTexture = inResources.GetBindlessHeapIndex(inData.mGBufferDepthTextureSRV),
            .mGbufferRenderTexture = inResources.GetBindlessHeapIndex(inData.mGBufferRenderTextureSRV),
            .mDispatchSize = viewport.GetRenderSize()
        });

        inCmdList->SetPipelineState(g_SystemShaders.mTraceShadowRaysShader.GetComputePSO());
        inCmdList->Dispatch(( viewport.GetRenderSize().x + 7 ) / 8, ( viewport.GetRenderSize().y + 7 ) / 8, 1);
    });

    return AddDenoisePasses(inRenderGraph, inDevice, inGBuffer, trace_data.mOutputTexture, "RT Shadows");
}



const RenderGraphResourceID AddAmbientOcclusionPass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferOutput& inGBuffer)
{
    const RTAOData& rtao_data = inRenderGraph.AddComputePass<RTAOData>("RTAO",
    [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, RTAOData& inData)
    {
        inData.mOutputTexture = inRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16_FLOAT,
            .width  = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = "RT_AORays"
        });

        inRGBuilder.Write(inData.mOutputTexture);

        inData.mGbufferDepthTextureSRV = inRGBuilder.Read(inGBuffer.mDepthTexture);
        inData.mGBufferRenderTextureSRV = inRGBuilder.Read(inGBuffer.mRenderTexture);
    },

    [&inRenderGraph, &inDevice](RTAOData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        const Viewport& viewport = inRenderGraph.GetViewport();

        inCmdList.PushComputeConstants(AmbientOcclusionRootConstants
        {
            .mAOmaskTexture           = inResources.GetBindlessHeapIndex(inData.mOutputTexture),
            .mGbufferDepthTexture     = inResources.GetBindlessHeapIndex(inData.mGbufferDepthTextureSRV),
            .mGbufferRenderTexture    = inResources.GetBindlessHeapIndex(inData.mGBufferRenderTextureSRV),
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
    });

    return AddDenoisePasses(inRenderGraph, inDevice, inGBuffer, rtao_data.mOutputTexture, "RTAO");
}



const RenderGraphResourceID AddDenoisePasses(RenderGraph& inRenderGraph, Device& inDevice, const GBufferOutput& inGBuffer, RenderGraphResourceID inSignalTexture, const String& inName)
{
    auto CreateDenoiseTexture = [&inRenderGraph](RenderGraphBuilder& inRGBuilder, const char* inDebugName, Texture::Usage inUsage = Texture::Usage::SHADER_READ_WRITE, DXGI_FORMAT inFormat = DXGI_FORMAT_R16G16B16A16_FLOAT)
    {
        return inRGBuilder.Create(Texture::Desc
        {
            .format = inFormat,
            .width  = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage  = inUsage,
            .debugName = inDebugName
        });
    };

    const DenoiseTemporalData& temporal_data = inRenderGraph.AddComputePass<DenoiseTemporalData>(inName + " Temporal",
    [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, DenoiseTemporalData& inData)
    {
        inData.mAccumulatedTexture = CreateDenoiseTexture(inRGBuilder, "RT_DenoiseAccumulated");
        inData.mHistoryTexture = CreateDenoiseTexture(inRGBuilder, "RT_DenoiseHistory", Texture::Usage::SHADER_READ_ONLY);

        inRGBuilder.Write(inData.mAccumulatedTexture);

        inData.mInputTextureSRV = inRGBuilder.Read(inSignalTexture);
        inData.mHistoryTextureSRV = inRGBuilder.Read(inData.mHistoryTexture);
        inData.mDepthTextureSRV = inRGBuilder.Read(inGBuffer.mDepthTexture);
        inData.mVelocityTextureSRV = inRGBuilder.Read(inGBuffer.mVelocityTexture);
    },

    [&inRenderGraph, &inDevice](DenoiseTemporalData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        const Viewport& viewport = inRenderGraph.GetViewport();

        inCmdList.PushComputeConstants(DenoiseRootConstants
        {
            .mInputTexture    = inResources.GetBindlessHeapIndex(inData.mInputTextureSRV),
            .mHistoryTexture  = inResources.GetBindlessHeapIndex(inData.mHistoryTextureSRV),
            .mOutputTexture   = inResources.GetBindlessHeapIndex(inData.mAccumulatedTexture),
            .mDepthTexture    = inResources.GetBindlessHeapIndex(inData.mDepthTextureSRV),
            .mVelocityTexture = inResources.GetBindlessHeapIndex(inData.mVelocityTextureSRV),
            .mDispatchSize    = viewport.GetRenderSize()
        });

        inCmdList->SetPipelineState(g_SystemShaders.mDenoiseTemporalShader.GetComputePSO());
        inCmdList->Dispatch(( viewport.GetRenderSize().x + 7 ) / 8, ( viewport.GetRenderSize().y + 7 ) / 8, 1);

        ID3D12Resource* accumulated_texture_resource = inDevice.GetD3D12Resource(inResources.GetTexture(inData.mAccumulatedTexture));
        ID3D12Resource* history_texture_resource = inDevice.GetD3D12Resource(inResources.GetTexture(inData.mHistoryTexture));

        std::array barriers =
        {
            D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(accumulated_texture_resource, GetD3D12ResourceStates(Texture::SHADER_READ_WRITE), D3D12_RESOURCE_STATE_COPY_SOURCE)),
            D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(history_texture_resource, GetD3D12ResourceStates(Texture::SHADER_READ_ONLY), D3D12_RESOURCE_STATE_COPY_DEST))
        };

        inCmdList->ResourceBarrier(barriers.size(), barriers.data());

        const CD3DX12_TEXTURE_COPY_LOCATION dest = CD3DX12_TEXTURE_COPY_LOCATION(history_texture_resource, 0);
        const CD3DX12_TEXTURE_COPY_LOCATION source = CD3DX12_TEXTURE_COPY_LOCATION(accumulated_texture_resource, 0);
        inCmdList->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);

        for (D3D12_RESOURCE_BARRIER& barrier : barriers)
            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

        inCmdList->ResourceBarrier(barriers.size(), barriers.data());
    });

    RenderGraphResourceID input_texture = temporal_data.mAccumulatedTexture;

    for (uint32_t iteration = 0; iteration < DENOISE_SPATIAL_ITERATIONS; iteration++)
    {
        RenderGraphResourceID output_texture;

        inRenderGraph.AddComputePass<DenoiseSpatialData>(std::format("{} Spatial {}", inName, iteration),
        [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, DenoiseSpatialData& inData)
        {
            const bool is_last_iteration = iteration + 1 == DENOISE_SPATIAL_ITERATIONS;
            output_texture = CreateDenoiseTexture(inRGBuilder, is_last_iteration ? "RT_Denoised" : "RT_DenoiseSpatial", Texture::Usage::SHADER_READ_WRITE, is_last_iteration ? DXGI_FORMAT_R16_FLOAT : DXGI_FORMAT_R16G16B16A16_FLOAT);

            inData.mStepSize = 1u << iteration;
            inData.mOutputTextureUAV = inRGBuilder.Write(output_texture);
            inData.mInputTextureSRV = inRGBuilder.Read(input_texture);
            inData.mGBufferTextureSRV = inRGBuilder.Read(inGBuffer.mRenderTexture);
        },

        [&inRenderGraph](DenoiseSpatialData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
        {
            const Viewport& viewport = inRenderGraph.GetViewport();

            inCmdList.PushComputeConstants(DenoiseRootConstants
            {
                .mInputTexture   = inResources.GetBindlessHeapIndex(inData.mInputTextureSRV),
                .mOutputTexture  = inResources.GetBindlessHeapIndex(inData.mOutputTextureUAV),
                .mGBufferTexture = inResources.GetBindlessHeapIndex(inData.mGBufferTextureSRV),
                .mStepSize       = inData.mStepSize,
                .mDispatchSize   = viewport.GetRenderSize()
            });

            inCmdList->SetPipelineState(g_SystemShaders.mDenoiseSpatialShader.GetComputePSO());
            inCmdList->Dispatch(( viewport.GetRenderSize().x + 7 ) / 8, ( viewport.GetRenderSize().y + 7 ) / 8, 1);
        });

        input_texture = output_texture;
    }

    return input_texture;
}



const ReflectionsData& AddReflectionsPass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferOutput& inGBuffer, const SkyCubeData& inSkyCubeData, const ConvolveCubeData& inConvolvedCubeData, const DDGIOutput* inDDGI)
{
    const UVec2 render_size = inRenderGraph.GetViewport().GetRenderSize();
    const uint32_t mip_count = glm::min(uint32_t(glm::floor(glm::log2(float(glm::max(render_size.x, render_size.y))))) + 1u, 7u);

    const ReflectionsData& reflections_data = inRenderGraph.AddComputePass<ReflectionsData>("RT Reflections",
    [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, ReflectionsData& inData)
    {
        inData.mOutputTexture = inRGBuilder.Create(Texture::Desc
        {
            .format    = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width     = render_size.x,
            .height    = render_size.y,
            .mipLevels = mip_count,
            .usage     = Texture::Usage::SHADER_READ_WRITE,
            .debugName = "RT_Reflections"
        });

        inRGBuilder.WriteTexture(inData.mOutputTexture, 0);

        inData.mSkyCubeTextureSRV = inRGBuilder.Read(inSkyCubeData.mSkyCubeTexture);
        inData.mDiffuseSkyCubeTextureSRV = inRGBuilder.Read(inConvolvedCubeData.mConvolvedCubeTexture);
        inData.mGBufferDepthTextureSRV = inRGBuilder.Read(inGBuffer.mDepthTexture);
        inData.mGbufferRenderTextureSRV = inRGBuilder.Read(inGBuffer.mRenderTexture);

        if (inDDGI)
        {
            inData.mUseDDGI = true;
            inData.mDDGIVolumesBufferSRV = inRGBuilder.Read(inDDGI->mVolumes);
            inData.mDDGIProbeDataBufferSRV = inRGBuilder.Read(inDDGI->mProbeData);
            inData.mDDGIDepthProbesSRV = inRGBuilder.Read(inDDGI->mDepthProbes);
            inData.mDDGIIrradianceProbesSRV = inRGBuilder.Read(inDDGI->mIrradianceProbes);
        }
    },

    [&inRenderGraph, &inDevice](ReflectionsData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        const Viewport& viewport = inRenderGraph.GetViewport();

        ReflectionsRootConstants root_constants =
        {
            .mResultTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mOutputTexture)),
            .mSkyCubeTexture = inRGResources.GetBindlessHeapIndex(inData.mSkyCubeTextureSRV),
            .mDiffuseSkyCubeTexture = inRGResources.GetBindlessHeapIndex(inData.mDiffuseSkyCubeTextureSRV),
            .mUseDDGI = inData.mUseDDGI,
            .mGbufferDepthTexture = inRGResources.GetBindlessHeapIndex(inData.mGBufferDepthTextureSRV),
            .mGbufferRenderTexture = inRGResources.GetBindlessHeapIndex(inData.mGbufferRenderTextureSRV),
            .mDispatchSize = viewport.GetRenderSize(),
            .mDDGIData = RenderSettings::GetDDGIData()
        };

        if (inData.mUseDDGI)
        {
            root_constants.mDDGIData.mVolumesBuffer = inRGResources.GetBindlessHeapIndex(inData.mDDGIVolumesBufferSRV);
            root_constants.mDDGIData.mProbesDataBuffer = inRGResources.GetBindlessHeapIndex(inData.mDDGIProbeDataBufferSRV);
            root_constants.mDDGIData.mProbesDepthTexture = inRGResources.GetBindlessHeapIndex(inData.mDDGIDepthProbesSRV);
            root_constants.mDDGIData.mProbesIrradianceTexture = inRGResources.GetBindlessHeapIndex(inData.mDDGIIrradianceProbesSRV);
        }

        inCmdList.PushComputeConstants(root_constants);
        inCmdList->SetPipelineState(g_SystemShaders.mRTReflectionsShader.GetComputePSO());
        inCmdList->Dispatch(( viewport.GetRenderSize().x + 7 ) / 8, ( viewport.GetRenderSize().y + 7 ) / 8, 1);
    });

    for (uint32_t mip = 1; mip < mip_count; mip++)
    {
        inRenderGraph.AddComputePass<BloomBlurData>(std::format("RT Reflections mip {} -> mip {}", mip - 1, mip),
        [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, BloomBlurData& inData)
        {
            inData.mToTextureMip   = mip;
            inData.mFromTextureMip = mip - 1;
            inData.mToTextureUAV   = inRGBuilder.WriteTexture(reflections_data.mOutputTexture, mip);
            inData.mFromTextureSRV = inRGBuilder.ReadTexture(reflections_data.mOutputTexture, mip - 1);
        },
        [&inDevice](BloomBlurData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
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

            inCmdList->SetPipelineState(g_SystemShaders.mBloomDownsampleShader.GetComputePSO());
            inCmdList->Dispatch(( UINT(to_viewport.Width) + 7 ) / 8, ( UINT(to_viewport.Height) + 7 ) / 8, 1);
        });
    }

    return reflections_data;
}



const PathTraceData& AddPathTracePass(RenderGraph& inRenderGraph, Device& inDevice, const SkyCubeData& inSkyCubeData, GBufferOutput& ioGBuffer)
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

    [&inRenderGraph, &inDevice](PathTraceData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        const Viewport& viewport = inRenderGraph.GetViewport();

        PathTraceRootConstants constants =
        {
            .mReset = RenderSettings::mPathTraceReset,
            .mBounces = RenderSettings::mPathTraceBounces,
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



DDGIOutput AddDDGIPass(RenderGraph& inRenderGraph, Device& inDevice, const GPUScene& inGPUScene, const GBufferOutput& inGBuffer, const SkyCubeData& inSkyCubeData)
{
    //////////////////////////////////////////
    ///// Probe Trace Compute Pass
    //////////////////////////////////////////
    struct ProbeTraceData
    {
        RenderGraphResourceID mVolumesBuffer;
        RenderGraphResourceViewID mVolumesBufferSRV;
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
        const int total_probe_count = RenderSettings::GetDDGITotalProbeCount();

        inData.mVolumesBuffer = ioRGBuilder.Create(Buffer::Desc
        {
            .size   = sizeof(DDGIVolume) * DDGI_MAX_CASCADES,
            .stride = sizeof(DDGIVolume),
            .debugName = "DDGI_Volumes"
        });

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

        inData.mVolumesBufferSRV = ioRGBuilder.Read(inData.mVolumesBuffer);
        inData.mProbeDataBufferSRV = ioRGBuilder.Read(inData.mProbeDataBuffer);
        inData.mProbesDepthTextureSRV = ioRGBuilder.Read(inData.mProbesDepthTexture);
        inData.mProbesIrradianceTextureSRV = ioRGBuilder.Read(inData.mProbesIrradianceTexture);

        ioRGBuilder.Write(inData.mRaysDepthTexture);
        ioRGBuilder.Write(inData.mRaysIrradianceTexture);

        inData.mSkyCubeTextureSRV = ioRGBuilder.Read(inSkyCubeData.mSkyCubeTexture);
    },
    [&inRenderGraph, &inDevice, &inGPUScene](ProbeTraceData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        Buffer& volumes_buffer = inDevice.GetBuffer(inRGResources.GetBuffer(inData.mVolumesBuffer));
        inDevice.UploadBufferData(inCmdList, volumes_buffer, 0, RenderSettings::mDDGIVolumes.data(), sizeof(DDGIVolume) * RenderSettings::mDDGIVolumes.size());

        const D3D12_RESOURCE_BARRIER volumes_barrier = CD3DX12_RESOURCE_BARRIER::Transition(volumes_buffer.GetD3D12Resource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
        inCmdList->ResourceBarrier(1, &volumes_barrier);

        if (!inGPUScene.HasTLAS())
            return;

        inData.mRandomRotationMatrix = gRandomOrientation();

        ProbeTraceRootConstants root_constants =
        {
            .mDebugProbeIndex = RenderSettings::GetDDGIDebugProbeIndex(),
            .mSkyCubeTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mSkyCubeTextureSRV)),
            .mRandomRotationMatrix = inData.mRandomRotationMatrix,
            .mDDGIData = RenderSettings::GetDDGIData()
        };

        root_constants.mDDGIData.mVolumesBuffer = inRGResources.GetBindlessHeapIndex(inData.mVolumesBufferSRV);
        root_constants.mDDGIData.mProbesDataBuffer = inRGResources.GetBindlessHeapIndex(inData.mProbeDataBufferSRV);
        root_constants.mDDGIData.mRaysDepthTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mRaysDepthTexture));
        root_constants.mDDGIData.mProbesDepthTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mProbesDepthTextureSRV));
        root_constants.mDDGIData.mRaysIrradianceTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTexture(inData.mRaysIrradianceTexture));
        root_constants.mDDGIData.mProbesIrradianceTexture = inDevice.GetBindlessHeapIndex(inRGResources.GetTextureView(inData.mProbesIrradianceTextureSRV));

        inCmdList.PushComputeConstants(root_constants);

        inCmdList->SetPipelineState(g_SystemShaders.mProbeTraceShader.GetComputePSO());
        inCmdList->Dispatch(DDGI_RAYS_PER_PROBE / DDGI_TRACE_SIZE, RenderSettings::GetDDGITotalProbeCount(), 1);
    });



    //////////////////////////////////////////
    ///// Probe Update Compute Pass
    //////////////////////////////////////////
    struct ProbeUpdateData
    {
        RenderGraphResourceViewID mVolumesBufferSRV;
        RenderGraphResourceViewID mProbesBufferUAV;
        RenderGraphResourceViewID mProbesDepthTextureUAV;
        RenderGraphResourceViewID mProbesIrradianceTextureUAV;
        RenderGraphResourceViewID mRaysDepthTextureSRV;
        RenderGraphResourceViewID mRaysIrradianceTextureSRV;
    };

    const ProbeUpdateData& update_data =  inRenderGraph.AddComputePass<ProbeUpdateData>("DDGI Update",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ProbeUpdateData& inData)
    {
        inData.mVolumesBufferSRV = ioRGBuilder.Read(trace_data.mVolumesBuffer);
        inData.mProbesBufferUAV = ioRGBuilder.Write(trace_data.mProbeDataBuffer);
        inData.mProbesDepthTextureUAV = ioRGBuilder.Write(trace_data.mProbesDepthTexture);
        inData.mProbesIrradianceTextureUAV = ioRGBuilder.Write(trace_data.mProbesIrradianceTexture);

        inData.mRaysDepthTextureSRV = ioRGBuilder.Read(trace_data.mRaysDepthTexture);
        inData.mRaysIrradianceTextureSRV = ioRGBuilder.Read(trace_data.mRaysIrradianceTexture);
    },

    [&inDevice, &trace_data, &inGPUScene](ProbeUpdateData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        if (!inGPUScene.HasTLAS())
            return;

        ProbeUpdateRootConstants root_constants =
        {
            .mRandomRotationMatrix = trace_data.mRandomRotationMatrix,
            .mDDGIData = RenderSettings::GetDDGIData()
        };

        root_constants.mDDGIData.mVolumesBuffer = inResources.GetBindlessHeapIndex(inData.mVolumesBufferSRV);
        root_constants.mDDGIData.mProbesDataBuffer = inResources.GetBindlessHeapIndex(inData.mProbesBufferUAV);
        root_constants.mDDGIData.mRaysDepthTexture = inResources.GetBindlessHeapIndex(inData.mRaysDepthTextureSRV);
        root_constants.mDDGIData.mProbesDepthTexture = inResources.GetBindlessHeapIndex(inData.mProbesDepthTextureUAV);
        root_constants.mDDGIData.mRaysIrradianceTexture = inResources.GetBindlessHeapIndex(inData.mRaysIrradianceTextureSRV);
        root_constants.mDDGIData.mProbesIrradianceTexture = inResources.GetBindlessHeapIndex(inData.mProbesIrradianceTextureUAV);

        inCmdList.PushComputeConstants(root_constants);

        const int total_probe_count = RenderSettings::GetDDGITotalProbeCount();

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
        RenderGraphResourceViewID mVolumesBufferSRV;
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
        inData.mVolumesBufferSRV = ioRGBuilder.Read(trace_data.mVolumesBuffer);
        inData.mProbeDataBufferSRV = ioRGBuilder.Read(trace_data.mProbeDataBuffer);
        inData.mProbesDepthTextureSRV = ioRGBuilder.Read(trace_data.mProbesDepthTexture);
        inData.mProbesIrradianceTextureSRV = ioRGBuilder.Read(trace_data.mProbesIrradianceTexture);
    },

    [&inDevice](ProbeSampleData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        const Texture& output_texture = inDevice.GetTexture(inResources.GetTexture(inData.mOutputTexture));

        ProbeSampleRootConstants root_constants = ProbeSampleRootConstants
        {
            .mDDGIData = RenderSettings::GetDDGIData(),
            .mOutputTexture = inResources.GetBindlessHeapIndex(inData.mOutputTexture),
            .mDepthTexture = inResources.GetBindlessHeapIndex(inData.mDepthTextureSRV),
            .mGBufferTexture = inResources.GetBindlessHeapIndex(inData.mGBufferTextureSRV),
            .mDispatchSize = UVec2(output_texture.GetWidth(), output_texture.GetHeight())
        };

        root_constants.mDDGIData.mVolumesBuffer = inResources.GetBindlessHeapIndex(inData.mVolumesBufferSRV);
        root_constants.mDDGIData.mProbesDataBuffer = inResources.GetBindlessHeapIndex(inData.mProbeDataBufferSRV);
        root_constants.mDDGIData.mProbesDepthTexture = inResources.GetBindlessHeapIndex(inData.mProbesDepthTextureSRV);
        root_constants.mDDGIData.mProbesIrradianceTexture = inResources.GetBindlessHeapIndex(inData.mProbesIrradianceTextureSRV);

        inCmdList->SetPipelineState(g_SystemShaders.mProbeSampleShader.GetComputePSO());
        inCmdList.PushComputeConstants(root_constants);

        inCmdList->Dispatch(( output_texture.GetWidth() + 7 ) / 8, ( output_texture.GetHeight() + 7 ) / 8, 1);
    });

    return DDGIOutput
    {
        .mOutput = sample_data.mOutputTexture,
        .mDepthProbes = trace_data.mProbesDepthTexture,
        .mIrradianceProbes = trace_data.mProbesIrradianceTexture,
        .mProbeData = trace_data.mProbeDataBuffer,
        .mVolumes = trace_data.mVolumesBuffer
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

        inData.mVolumesBufferSRV = ioRGBuilder.Read(inDDGI.mVolumes);
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

        DDGIData ddgi_data = RenderSettings::GetDDGIData();
        ddgi_data.mVolumesBuffer = inResources.GetBindlessHeapIndex(inData.mVolumesBufferSRV);
        ddgi_data.mProbesDataBuffer = inResources.GetBindlessHeapIndex(inData.mProbeDataBufferSRV);
        ddgi_data.mProbesDepthTexture = inResources.GetBindlessHeapIndex(inData.mProbesDepthTextureSRV);
        ddgi_data.mProbesIrradianceTexture = inResources.GetBindlessHeapIndex(inData.mProbesIrradianceTextureSRV);

        inCmdList.PushGraphicsConstants(ddgi_data);

        inCmdList.BindVertexAndIndexBuffers(inDevice, *inData.mProbeMesh);
        inCmdList->DrawIndexedInstanced(inData.mProbeMesh->indices.size(), RenderSettings::GetDDGITotalProbeCount(), 0, 0, 0);
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
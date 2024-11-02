#pragma once

#include "Components.h"
#include "RenderGraph.h"
#include "RenderPasses.h"
#include "RayTracedScene.h"

namespace RK::DX12 {

struct GBufferData;

////////////////////////////////////////
/// Ray-traced Shadows Compute Passes
////////////////////////////////////////
struct TraceShadowTilesData
{
    RTTI_DECLARE_TYPE(TraceShadowTilesData);

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mGBufferDepthTextureSRV;
    RenderGraphResourceViewID mGBufferRenderTextureSRV;
};

struct ClearShadowTilesData
{
    RTTI_DECLARE_TYPE(ClearShadowTilesData);

    RenderGraphResourceID mTilesBuffer;
    RenderGraphResourceID mIndirectDispatchBuffer;
};

struct ClassifyShadowTilesData
{
    RTTI_DECLARE_TYPE(ClassifyShadowTilesData);

    RenderGraphResourceViewID mTilesBufferUAV;
    RenderGraphResourceViewID mIndirectDispatchBufferUAV;
    RenderGraphResourceViewID mTracedShadowRaysTextureSRV;
};

struct ClearShadowsData
{
    RTTI_DECLARE_TYPE(ClearShadowsData);

    RenderGraphResourceID mShadowsTexture;
    RenderGraphResourceID mShadowsTextureHistory;
};

struct DenoiseShadowsData
{
    RTTI_DECLARE_TYPE(DenoiseShadowsData);

    RenderGraphResourceViewID mOutputTextureUAV;
    RenderGraphResourceViewID mHistoryTextureUAV;
    RenderGraphResourceViewID mDepthTextureSRV;
    RenderGraphResourceViewID mGBufferTextureSRV;
    RenderGraphResourceViewID mVelocityTextureSRV;
    RenderGraphResourceViewID mSelectionTextureSRV;
    RenderGraphResourceViewID mTracedShadowRaysTextureSRV;
    RenderGraphResourceViewID mTilesBufferSRV;
    RenderGraphResourceViewID mDenoisedTilesBufferSRV;
    RenderGraphResourceViewID mIndirectDispatchBufferSRV;
    RenderGraphResourceViewID mDenoisedIndirectDispatchBufferSRV;
};


const RenderGraphResourceID AddRayTracedShadowsPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene,
    const GBufferOutput& inGBuffer
);



//////////////////////////////////////////
///// Ray-traced Ambient Occlusion Render Pass
//////////////////////////////////////////
struct RTAOData
{
    RTTI_DECLARE_TYPE(RTAOData);

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mGbufferDepthTextureSRV;
    RenderGraphResourceViewID mGBufferRenderTextureSRV;
};

const RTAOData& AddAmbientOcclusionPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene,
    const GBufferOutput& inGBuffer
);



//////////////////////////////////////////
///// Ray-traced Reflections Compute Pass
//////////////////////////////////////////
struct ReflectionsData
{
    RTTI_DECLARE_TYPE(ReflectionsData);

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mSkyCubeTextureSRV;
    RenderGraphResourceViewID mGBufferDepthTextureSRV;
    RenderGraphResourceViewID mGbufferRenderTextureSRV;
};

const ReflectionsData& AddReflectionsPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene,
    const GBufferOutput& inGBuffer,
    const SkyCubeData& inSkyCubeData
);



//////////////////////////////////////////
///// Reference Path-Tracing Compute Pass
//////////////////////////////////////////
struct PathTraceData
{
    RTTI_DECLARE_TYPE(PathTraceData);

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceID mAccumulationTexture;
    RenderGraphResourceViewID mSkyCubeTextureSRV;
};

const PathTraceData& AddPathTracePass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene,
    const SkyCubeData& inSkyCubeData
);



//////////////////////////////////////////
///// DDGI Pass
//////////////////////////////////////////
struct DDGIOutput
{
    RenderGraphResourceID mOutput;
    RenderGraphResourceID mDepthProbes;
    RenderGraphResourceID mIrradianceProbes;
};

DDGIOutput AddDDGIPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene, const GBufferOutput& inGBuffer, const SkyCubeData& inSkyCubeData);



//////////////////////////////////////////
///// GI Probe Debug Render Pass
//////////////////////////////////////////
struct ProbeDebugData
{
    RTTI_DECLARE_TYPE(ProbeDebugData);

    RK::Mesh mProbeMesh;
    RenderGraphResourceViewID mRenderTargetRTV;
    RenderGraphResourceViewID mDepthTargetDSV;
    RenderGraphResourceViewID mProbesDepthTextureSRV;
    RenderGraphResourceViewID mProbesIrradianceTextureSRV;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ProbeDebugData& AddProbeDebugPass(RenderGraph& inRenderGraph, Device& inDevice,
    const DDGIOutput& inDDGI,
    RenderGraphResourceID inRenderTarget,
    RenderGraphResourceID inDepthTarget
);



//////////////////////////////////////////
///// GI Probe Debug Rays Render Pass
//////////////////////////////////////////
struct ProbeDebugRaysData
{
    RTTI_DECLARE_VIRTUAL_TYPE(ProbeDebugRaysData);

    RenderGraphResourceID mVertexBuffer;
    RenderGraphResourceID mIndirectArgsBuffer;
    ComPtr<ID3D12PipelineState> mPipeline;
    ComPtr<ID3D12CommandSignature> mCommandSignature;
};

const ProbeDebugRaysData& AddProbeDebugRaysPass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID inRenderTarget,
    RenderGraphResourceID inDepthTarget
);

} // Raekor
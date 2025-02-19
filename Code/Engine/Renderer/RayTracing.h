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
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mGBufferDepthTextureSRV;
    RenderGraphResourceViewID mGBufferRenderTextureSRV;
};

struct ClearShadowTilesData
{
    RenderGraphResourceID mTilesBuffer;
    RenderGraphResourceID mIndirectDispatchBuffer;
};

struct ClassifyShadowTilesData
{
    RenderGraphResourceViewID mTilesBufferUAV;
    RenderGraphResourceViewID mIndirectDispatchBufferUAV;
    RenderGraphResourceViewID mTracedShadowRaysTextureSRV;
};

struct ClearShadowsData
{
    RenderGraphResourceID mShadowsTexture;
    RenderGraphResourceID mShadowsTextureHistory;
};

struct DenoiseShadowsData
{
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
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceID mHistoryTexture;
    RenderGraphResourceViewID mHistoryTextureSRV;
    RenderGraphResourceViewID mGbufferDepthTextureSRV;
    RenderGraphResourceViewID mGBufferRenderTextureSRV;
    RenderGraphResourceViewID mGBufferVelocityTextureSRV;
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
    UVec2 mViewport;
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
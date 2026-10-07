#pragma once

#include "Components.h"
#include "RenderGraph.h"
#include "RenderPasses.h"
#include "GPUScene.h"
#include "RenderWorld.h"

namespace RK::DX12 {

struct GBufferData;

struct BuildAccelerationStructuresData
{
};

const BuildAccelerationStructuresData& AddBuildAccelerationStructuresPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RenderWorld& inWorld,
    GPUScene& inGPUScene
);


////////////////////////////////////////
/// Ray-traced Shadows Compute Passes
////////////////////////////////////////
struct TraceShadowsData
{
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mGBufferDepthTextureSRV;
    RenderGraphResourceViewID mGBufferRenderTextureSRV;
};


const RenderGraphResourceID AddRayTracedShadowsPass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferOutput& inGBuffer
);



//////////////////////////////////////////
///// Ray-traced Ambient Occlusion Render Pass
//////////////////////////////////////////
struct RTAOData
{
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mGbufferDepthTextureSRV;
    RenderGraphResourceViewID mGBufferRenderTextureSRV;
};

const RenderGraphResourceID AddAmbientOcclusionPass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferOutput& inGBuffer
);



struct DenoiseTemporalData
{
    RenderGraphResourceID mAccumulatedTexture;
    RenderGraphResourceID mHistoryTexture;
    RenderGraphResourceViewID mInputTextureSRV;
    RenderGraphResourceViewID mHistoryTextureSRV;
    RenderGraphResourceViewID mDepthTextureSRV;
    RenderGraphResourceViewID mVelocityTextureSRV;
};

struct DenoiseSpatialData
{
    uint32_t mStepSize = 1;
    RenderGraphResourceViewID mInputTextureSRV;
    RenderGraphResourceViewID mOutputTextureUAV;
    RenderGraphResourceViewID mGBufferTextureSRV;
};

const RenderGraphResourceID AddDenoisePasses(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferOutput& inGBuffer,
    RenderGraphResourceID inSignalTexture,
    const String& inName
);



//////////////////////////////////////////
///// Ray-traced Reflections Compute Pass
//////////////////////////////////////////
struct DDGIOutput;

struct ReflectionsData
{
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mSkyCubeTextureSRV;
    RenderGraphResourceViewID mDiffuseSkyCubeTextureSRV;
    RenderGraphResourceViewID mGBufferDepthTextureSRV;
    RenderGraphResourceViewID mGbufferRenderTextureSRV;
    RenderGraphResourceViewID mDDGIVolumesBufferSRV;
    RenderGraphResourceViewID mDDGIProbeDataBufferSRV;
    RenderGraphResourceViewID mDDGIDepthProbesSRV;
    RenderGraphResourceViewID mDDGIIrradianceProbesSRV;
    bool mUseDDGI = false;
};

const ReflectionsData& AddReflectionsPass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferOutput& inGBuffer,
    const SkyCubeData& inSkyCubeData,
    const ConvolveCubeData& inConvolvedCubeData,
    const DDGIOutput* inDDGI
);



//////////////////////////////////////////
///// Reference Path-Tracing Compute Pass
//////////////////////////////////////////
struct PathTraceData
{
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceID mGBufferTexture;;
    RenderGraphResourceID mAccumulationTexture;
    RenderGraphResourceID mDepthTexture;
    RenderGraphResourceID mDepthWriteTexture;
    RenderGraphResourceID mSelectionTexture;
    RenderGraphResourceViewID mSkyCubeTextureSRV;
};

const PathTraceData& AddPathTracePass(RenderGraph& inRenderGraph, Device& inDevice,
    const SkyCubeData& inSkyCubeData,
    GBufferOutput& ioGBuffer
);



//////////////////////////////////////////
///// DDGI Pass
//////////////////////////////////////////
struct DDGIOutput
{
    RenderGraphResourceID mOutput;
    RenderGraphResourceID mDepthProbes;
    RenderGraphResourceID mIrradianceProbes;
    RenderGraphResourceID mProbeData;
    RenderGraphResourceID mVolumes;
};

DDGIOutput AddDDGIPass(RenderGraph& inRenderGraph, Device& inDevice, const GPUScene& inGPUScene, const GBufferOutput& inGBuffer, const SkyCubeData& inSkyCubeData);



//////////////////////////////////////////
///// GI Probe Debug Render Pass
//////////////////////////////////////////
struct ProbeDebugData
{
    UVec2 mViewport;
    const RK::Mesh* mProbeMesh = nullptr;
    RenderGraphResourceViewID mRenderTargetRTV;
    RenderGraphResourceViewID mDepthTargetDSV;
    RenderGraphResourceViewID mVolumesBufferSRV;
    RenderGraphResourceViewID mProbeDataBufferSRV;
    RenderGraphResourceViewID mProbesDepthTextureSRV;
    RenderGraphResourceViewID mProbesIrradianceTextureSRV;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ProbeDebugData& AddProbeDebugPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RK::Mesh& inProbeMesh,
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
    RenderGraphResourceViewID mVertexBufferSRV;
    RenderGraphResourceID mIndirectArgsBuffer;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ProbeDebugRaysData& AddProbeDebugRaysPass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID inRenderTarget,
    RenderGraphResourceID inDepthTarget,
    BufferID inLinesVertexBuffer,
    BufferID inIndirectArgsBuffer
);

} // Raekor
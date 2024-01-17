#pragma once

#include "DXScene.h"
#include "DXRenderGraph.h"

namespace Raekor::DX12 {

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
    RenderGraphResourceViewID mTracedShadowRaysTextureSRV;
    RenderGraphResourceViewID mTilesBufferSRV;
    RenderGraphResourceViewID mDenoisedTilesBufferSRV;
    RenderGraphResourceViewID mIndirectDispatchBufferSRV;
    RenderGraphResourceViewID mDenoisedIndirectDispatchBufferSRV;
};


const RenderGraphResourceID AddRayTracedShadowsPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene,
    const GBufferData& inGBufferData
);



//////////////////////////////////////////
///// Ray-traced Ambient Occlusion Render Pass
//////////////////////////////////////////
struct RTAOData
{
    RTTI_DECLARE_TYPE(RTAOData);

    static inline AmbientOcclusionParams mParams =
    {
        .mRadius = 2.0,
        .mPower = 1.0,
        .mNormalBias = 0.01,
        .mSampleCount = 1u
    };

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mGbufferDepthTextureSRV;
    RenderGraphResourceViewID mGBufferRenderTextureSRV;
};

const RTAOData& AddAmbientOcclusionPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene,
    const GBufferData& inGBufferData
);



//////////////////////////////////////////
///// Ray-traced Reflections Compute Pass
//////////////////////////////////////////
struct ReflectionsData
{
    RTTI_DECLARE_TYPE(ReflectionsData);

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mGBufferDepthTextureSRV;
    RenderGraphResourceViewID mGbufferRenderTextureSRV;
};

const ReflectionsData& AddReflectionsPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene,
    const GBufferData& inGBufferData
);



//////////////////////////////////////////
///// Reference Path-Tracing Compute Pass
//////////////////////////////////////////
struct PathTraceData
{
    RTTI_DECLARE_TYPE(PathTraceData);

    static inline bool mReset = true;
    static inline uint32_t mBounces = 2;
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceID mAccumulationTexture;
};

const PathTraceData& AddPathTracePass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene
);



//////////////////////////////////////////
///// GI Probe Trace Compute Pass
//////////////////////////////////////////
struct ProbeTraceData
{
    RTTI_DECLARE_TYPE(ProbeTraceData);

    static inline IVec3 mDebugProbe = IVec3(10, 10, 5);
    static inline DDGIData mDDGIData =
    {
        .mProbeCount = IVec3(22, 22, 22),
        .mProbeRadius = 1.0f,
        .mProbeSpacing = Vec3(6.4, 3.0, 2.8),
        .mCornerPosition = Vec3(-65, -1.4, -28.5)
    };

    Mat4x4 mRandomRotationMatrix;
    RenderGraphResourceID mProbesDepthTexture;
    RenderGraphResourceID mProbesIrradianceTexture;
    RenderGraphResourceID mRaysDepthTexture;
    RenderGraphResourceID mRaysIrradianceTexture;
};

const ProbeTraceData& AddProbeTracePass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene
);



//////////////////////////////////////////
///// GI Probe Update Compute Pass
//////////////////////////////////////////
struct ProbeUpdateData
{
    RTTI_DECLARE_TYPE(ProbeUpdateData);

    static inline bool mClear = false;
    DDGIData mDDGIData;
    RenderGraphResourceID mProbesDepthTexture;
    RenderGraphResourceID mProbesIrradianceTexture;
    RenderGraphResourceViewID mRaysDepthTextureSRV;
    RenderGraphResourceViewID mRaysIrradianceTextureSRV;
};

const ProbeUpdateData& AddProbeUpdatePass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene,
    const ProbeTraceData& inTraceData
);



//////////////////////////////////////////
///// GI Probe Sample Compute Pass
//////////////////////////////////////////
struct ProbeSampleData
{
    RTTI_DECLARE_TYPE(ProbeSampleData);

    DDGIData mDDGIData;
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mDepthTextureSRV;
    RenderGraphResourceViewID mGBufferTextureSRV;
    RenderGraphResourceViewID mProbesDepthTextureSRV;
    RenderGraphResourceViewID mProbesIrradianceTextureSRV;
};

const ProbeSampleData& AddProbeSamplePass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferData& inGBufferData,
    const ProbeUpdateData& inUpdateData
);



//////////////////////////////////////////
///// GI Probe Debug Render Pass
//////////////////////////////////////////
struct ProbeDebugData
{
    RTTI_DECLARE_TYPE(ProbeDebugData);

    static inline float mRadius = 1.0f;
    Mesh            mProbeMesh;
    DDGIData        mDDGIData;
    RenderGraphResourceViewID mRenderTargetRTV;
    RenderGraphResourceViewID mDepthTargetDSV;
    RenderGraphResourceViewID mProbesDepthTextureSRV;
    RenderGraphResourceViewID mProbesIrradianceTextureSRV;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ProbeDebugData& AddProbeDebugPass(RenderGraph& inRenderGraph, Device& inDevice,
    const ProbeTraceData& inTraceData,
    const ProbeUpdateData& inUpdateData,
    RenderGraphResourceID inRenderTarget,
    RenderGraphResourceID inDepthTarget
);



//////////////////////////////////////////
///// GI Probe Debug Rays Render Pass
//////////////////////////////////////////
struct ProbeDebugRaysData
{
    RTTI_DECLARE_TYPE(ProbeDebugRaysData);

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
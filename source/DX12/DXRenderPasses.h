#pragma once

#include "DXScene.h"
#include "DXRenderGraph.h"

namespace Raekor::DX12 {

enum EDebugTexture
{
    DEBUG_TEXTURE_NONE = 0,
    DEBUG_TEXTURE_GBUFFER_DEPTH,
    DEBUG_TEXTURE_GBUFFER_ALBEDO,
    DEBUG_TEXTURE_GBUFFER_NORMALS,
    DEBUG_TEXTURE_GBUFFER_VELOCITY,
    DEBUG_TEXTURE_GBUFFER_METALLIC,
    DEBUG_TEXTURE_GBUFFER_ROUGHNESS,
    DEBUG_TEXTURE_LIGHTING,
    DEBUG_TEXTURE_RT_SHADOWS,
    DEBUG_TEXTURE_RT_REFLECTIONS,
    DEBUG_TEXTURE_RT_AMBIENT_OCCLUSION,
    DEBUG_TEXTURE_COUNT
};


////////////////////////////////////////
/// Clear BufferPass
////////////////////////////////////////
struct ClearBufferData
{
    RTTI_DECLARE_TYPE(ClearBufferData);

    RenderGraphResourceViewID mBufferUAV;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ClearBufferData& AddClearBufferPass(RenderGraph& inRenderGraph, Device& inDevice,
    BufferID inTexture, 
    uint32_t inValue
);



////////////////////////////////////////
/// Clear Texture Float Pass
////////////////////////////////////////
struct ClearTextureFloatData
{
    RTTI_DECLARE_TYPE(ClearTextureFloatData);

    RenderGraphResourceViewID mTextureUAV;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ClearTextureFloatData& AddClearTextureFloatPass(RenderGraph& inRenderGraph, Device& inDevice,
    TextureID inTexture,
    const Vec4& inClearValue
);



////////////////////////////////////////
/// Compute Sky Cube Pass
////////////////////////////////////////
struct SkyCubeData
{
    RTTI_DECLARE_TYPE(SkyCubeData);

    RenderGraphResourceID mSkyCubeTexture;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const SkyCubeData& AddSkyCubePass(RenderGraph& inRenderGraph, Device& inDevice,
    const Scene& inScene,
    TextureID inSkyCubeTexture
);



////////////////////////////////////////
/// Convolve Irradiance Cubemap Cube Pass
////////////////////////////////////////
struct ConvolveCubeData
{
    RTTI_DECLARE_TYPE(ConvolveCubeData);

    RenderGraphResourceID mConvolvedCubeTexture;
    RenderGraphResourceViewID mCubeTexture;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ConvolveCubeData& AddConvolveCubePass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID inCubeTexture,
    TextureID inConvolvedCubeTexture
);



////////////////////////////////////////
/// Meshlets Raster Render Pass
////////////////////////////////////////
struct MeshletsRasterData
{
    RTTI_DECLARE_TYPE(MeshletsRasterData);

    Entity mActiveEntity = NULL_ENTITY;
    RenderGraphResourceID mDepthTexture;
    RenderGraphResourceID mRenderTexture;
    RenderGraphResourceID mVelocityTexture;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const MeshletsRasterData& AddMeshletsRasterPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene
);



////////////////////////////////////////
/// GBuffer Render Pass
////////////////////////////////////////
struct GBufferData
{
    RTTI_DECLARE_TYPE(GBufferData);

    Entity mActiveEntity = NULL_ENTITY;
    RenderGraphResourceID mDepthTexture;
    RenderGraphResourceID mRenderTexture;
    RenderGraphResourceID mVelocityTexture;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const GBufferData& AddGBufferPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene
);


//////////////////////////////////////////
///// GBuffer Debug Pass
//////////////////////////////////////////
struct GBufferDebugData
{
    RTTI_DECLARE_TYPE(GBufferDebugData);

    GBufferData mGBufferData;
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mInputTextureSRV;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const GBufferDebugData& AddGBufferDebugPass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferData& inGBufferData,
    EDebugTexture inDebugTexture
);


//////////////////////////////////////////
///// Grass Pass
//////////////////////////////////////////
struct GrassData
{
    RTTI_DECLARE_TYPE(GrassData);

    BufferID mPerBladeIndexBuffer;
    RenderGraphResourceViewID mDepthTextureSRV;
    RenderGraphResourceViewID mRenderTextureSRV;
    GrassRenderRootConstants mRenderConstants;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const GrassData& AddGrassRenderPass(RenderGraph& inGraph, Device& inDevice,
    const GBufferData& inGBufferData
);


////////////////////////////////////////
/// Ray-traced Mask Render Passes
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
        .mPower = 0.0,
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
///// Ray-traced Reflections Render Pass
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
///// Ray-traced Indirect Diffuse Render Pass
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
///// Downsample Render Pass
//////////////////////////////////////////
struct DownsampleData
{
    RTTI_DECLARE_TYPE(DownsampleData);

    RenderGraphResourceID mGlobalAtomicBuffer;
    RenderGraphResourceViewID mSourceTextureUAV;
    RenderGraphResourceViewID mSourceTextureMipsUAVs[12];
    ComPtr<ID3D12PipelineState> mPipeline;
};

const DownsampleData& AddDownsamplePass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID inSourceTexture
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

    DDGIData        mDDGIData;
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
///// Debug Lines Render Pass
//////////////////////////////////////////
struct DebugLinesData
{
    RTTI_DECLARE_TYPE(DebugLinesData);

    D3D12_DRAW_ARGUMENTS* mMappedPtr;
    RenderGraphResourceID mVertexBuffer;
    RenderGraphResourceID mIndirectArgsBuffer;
    ComPtr<ID3D12PipelineState> mPipeline;
    ComPtr<ID3D12CommandSignature> mCommandSignature;
};

const DebugLinesData& AddDebugLinesPass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID inRenderTarget,
    RenderGraphResourceID inDepthTarget
);


//////////////////////////////////////////
///// Deferred Lighting Render Pass
//////////////////////////////////////////
struct LightingData
{
    RTTI_DECLARE_TYPE(LightingData);

    DDGIData        mDDGIData;
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mShadowMaskTextureSRV;
    RenderGraphResourceViewID mReflectionsTextureSRV;
    RenderGraphResourceViewID mGBufferDepthTextureSRV;
    RenderGraphResourceViewID mGBufferRenderTextureSRV;
    RenderGraphResourceViewID mAmbientOcclusionTextureSRV;
    RenderGraphResourceViewID mProbesDepthTextureSRV;
    RenderGraphResourceViewID mProbesIrradianceTextureSRV;
    RenderGraphResourceViewID mIndirectDiffuseTextureSRV;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const LightingData& AddLightingPass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferData& inGBufferData,
    RenderGraphResourceID inShadowTexture,
    const ReflectionsData& inReflectionsData,
    RenderGraphResourceID inAOTexture,
    const ProbeUpdateData& inProbeData
);


//////////////////////////////////////////
///// AMD FSR 2.1 Render Pass
//////////////////////////////////////////
struct FSR2Data
{
    RTTI_DECLARE_TYPE(FSR2Data);

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mColorTextureSRV;
    RenderGraphResourceViewID mDepthTextureSRV;
    RenderGraphResourceViewID mMotionVectorTextureSRV;
    float mDeltaTime = 0;
    uint32_t mFrameCounter = 0;
};

const FSR2Data& AddFsrPass(RenderGraph& inRenderGraph, Device& inDevice,
    FfxFsr2Context& inContext,
    RenderGraphResourceID inColorTexture,
    const GBufferData& inGBufferData
);


//////////////////////////////////////////
///// Nvidia DLSS Render Pass
//////////////////////////////////////////
struct DLSSData
{
    RTTI_DECLARE_TYPE(DLSSData);

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mColorTextureSRV;
    RenderGraphResourceViewID mDepthTextureSRV;
    RenderGraphResourceViewID mMotionVectorTextureSRV;
    uint32_t mFrameCounter = 0;
};

const DLSSData& AddDLSSPass(RenderGraph& inRenderGraph, Device& inDevice,
    NVSDK_NGX_Handle* inDLSSHandle,
    NVSDK_NGX_Parameter* inDLSSParams,
    RenderGraphResourceID inColorTexture,
    const GBufferData& inGBufferData
);


//////////////////////////////////////////
///// Intel XeSS Render Pass
//////////////////////////////////////////
struct XeSSData
{
    RTTI_DECLARE_TYPE(XeSSData);

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mColorTextureSRV;
    RenderGraphResourceViewID mDepthTextureSRV;
    RenderGraphResourceViewID mMotionVectorTextureSRV;
    uint32_t mFrameCounter = 0;
};

const XeSSData& AddXeSSPass(RenderGraph& inRenderGraph, Device& inDevice,
    xess_context_handle_t inContext,
    RenderGraphResourceID inColorTexture,
    const GBufferData& inGBufferData
);


//////////////////////////////////////////
///// TAA Resolve Render Pass
//////////////////////////////////////////
struct TAAResolveData
{
    RTTI_DECLARE_TYPE(TAAResolveData);

    uint32_t mFrameCounter = 0;
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceID mHistoryTexture;
    RenderGraphResourceViewID mHistoryTextureSRV;
    RenderGraphResourceViewID mColorTextureSRV;
    RenderGraphResourceViewID mDepthTextureSRV;
    RenderGraphResourceViewID mVelocityTextureSRV;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const TAAResolveData& AddTAAResolvePass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferData& inGBufferData,
    RenderGraphResourceID inColorTexture,
    uint32_t inFrameCounter
);


////////////////////////////////////////
/// Depth of Field Render Pass
////////////////////////////////////////
struct DepthOfFieldData
{
    RTTI_DECLARE_TYPE(DepthOfFieldData);

    static inline float mFocusPoint = 1.0f;
    static inline float mFocusScale = 1.0f;
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mDepthTextureSRV;
    RenderGraphResourceViewID mInputTextureSRV;
};

const DepthOfFieldData& AddDepthOfFieldPass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID inInputTexture,
    RenderGraphResourceID mInDepthTexture
);


////////////////////////////////////////
/// Final Compose Render Pass
////////////////////////////////////////
struct ComposeData
{
    RTTI_DECLARE_TYPE(ComposeData);

    static inline float mExposure = 1.0f;
    static inline float mBloomBlendFactor = 0.06f;
    static inline float mChromaticAberrationStrength = 0.0f;
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mInputTextureSRV;
    RenderGraphResourceViewID mBloomTextureSRV;;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ComposeData& AddComposePass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID inBloomTexture,
    RenderGraphResourceID inInputTexture
);


////////////////////////////////////////
/// Bloom Downscale and Upscale passes
////////////////////////////////////////
struct BloomDownscaleData
{
    RTTI_DECLARE_TYPE(BloomDownscaleData);
    uint32_t mToTextureMip = 0;
    uint32_t mFromTextureMip = 0;
    RenderGraphResourceViewID mToTextureUAV;
    RenderGraphResourceViewID mFromTextureSRV;
};


struct BloomData
{
    RTTI_DECLARE_TYPE(BloomData);

    RenderGraphResourceID mOutputTexture;
};


const BloomData& AddBloomPass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID inInputTexture
);


////////////////////////////////////////
/// Pre-ImGui Pass
////////////////////////////////////////
struct PreImGuiData
{
    RTTI_DECLARE_TYPE(PreImGuiData);

    RenderGraphResourceViewID mDisplayTextureSRV;
};

const PreImGuiData& AddPreImGuiPass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID ioDisplayTexture
);


////////////////////////////////////////
/// ImGui Pass
////////////////////////////////////////
struct ImGuiData
{
    RTTI_DECLARE_TYPE(ImGuiData);

    RenderGraphResourceID mIndexBuffer;
    RenderGraphResourceID mVertexBuffer;
    RenderGraphResourceViewID mBackBufferRTV;
    RenderGraphResourceViewID mInputTextureSRV;
    std::vector<uint8_t> mIndexScratchBuffer;
    std::vector<uint8_t> mVertexScratchBuffer;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ImGuiData& AddImGuiPass(RenderGraph& inRenderGraph, Device& inDevice,
    StagingHeap& inStagingHeap,
    RenderGraphResourceID inInputTexture,
    TextureID inBackBuffer
);

}
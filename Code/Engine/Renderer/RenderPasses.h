#pragma once

#include "RenderGraph.h"
#include "RayTracedScene.h"

namespace RK::DX12 {

struct RenderSettings
{
    static inline float mDoFFocusPoint = 1.0f;
    static inline float mDoFFocusScale = 1.0f;

    static inline int mSSAOSamples = 16;
    static inline float mSSAOBias = 0.025f;
    static inline float mSSAORadius = 0.05f;

    static inline int mSSRSamples = 16;
    static inline float mSSRBias = 0.025f;
    static inline float mSSRRadius = 0.05f;

    static inline float mGrassBend = 0.0f;
    static inline float mGrassTilt = 0.0f;
    static inline Vec2 mWindDirection = Vec2(0.0f, -1.0f);

    static inline float mExposure = 1.0f;
    static inline float mVignetteScale = 0.8f;
    static inline float mVignetteBias = 0.2f;
    static inline float mVignetteInner = 0.0f;
    static inline float mVignetteOuter = 2.0f;
    static inline float mBloomBlendFactor = 0.06f;
    static inline float mChromaticAberrationStrength = 0.0;

    static inline float mRTAORadius = 1.0;
    static inline float mRTAOPower = 1.0;
    static inline float mRTAONormalBias = 0.01;
    static inline uint32_t mRTAOSampleCount = 1;

    static inline bool mPathTraceReset = false;
    static inline uint32_t mPathTraceBounces = 2u;

    static inline float mDDGIDebugRadius = 1.0f;
    static inline IVec3 mDDGIDebugProbe = IVec3(10, 10, 5);
    static inline IVec3 mDDGIProbeCount = IVec3(22, 22, 22);
    static inline Vec3 mDDGIProbeSpacing = Vec3(6.4, 3.0, 2.8);
    static inline Vec3 mDDGICornerPosition = Vec3(-65, -1.4, -28.5);

    static inline Entity mActiveEntity = Entity::Null;
};


enum EDebugTexture
{
    DEBUG_TEXTURE_NONE = 0,
    DEBUG_TEXTURE_GBUFFER_DEPTH,
    DEBUG_TEXTURE_GBUFFER_ALBEDO,
    DEBUG_TEXTURE_GBUFFER_NORMALS,
    DEBUG_TEXTURE_GBUFFER_EMISSIVE,
    DEBUG_TEXTURE_GBUFFER_VELOCITY,
    DEBUG_TEXTURE_GBUFFER_METALLIC,
    DEBUG_TEXTURE_GBUFFER_ROUGHNESS,
    DEBUG_TEXTURE_LIGHTING,
    DEBUG_TEXTURE_SSR,
    DEBUG_TEXTURE_SSAO,
    DEBUG_TEXTURE_RT_SHADOWS,
    DEBUG_TEXTURE_RT_REFLECTIONS,
    DEBUG_TEXTURE_RT_INDIRECT_DIFFUSE,
    DEBUG_TEXTURE_RT_AMBIENT_OCCLUSION,
    DEBUG_TEXTURE_COUNT
};


////////////////////////////////////////
/// Defaults Pass
////////////////////////////////////////
struct DefaultTexturesData
{
    RTTI_DECLARE_TYPE(DefaultTexturesData);

    RenderGraphResourceID mBlackTexture;
    RenderGraphResourceID mWhiteTexture;
};

const DefaultTexturesData& AddDefaultTexturesPass(RenderGraph& inRenderGraph, Device& inDevice,
    TextureID inBlackTexture,
    TextureID inWhiteTexture
);



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
/// Transition Resource Pass
////////////////////////////////////////
struct TransitionResourceData
{
    RTTI_DECLARE_TYPE(TransitionResourceData);
    RenderGraphResourceViewID mResourceView;
};

typedef RenderGraphResourceViewID ( RenderGraphBuilder::*RenderGraphBuilderFunction ) ( RenderGraphResourceID );

const TransitionResourceData& AddTransitionResourceData(RenderGraph& inRenderGraph, Device& inDevice, 
    RenderGraphBuilderFunction inFunction,
    RenderGraphResourceID inResource
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
    const Scene& inScene
);



////////////////////////////////////////
/// Convolve Irradiance Cubemap Cube Pass
////////////////////////////////////////
struct ConvolveCubeData
{
    RTTI_DECLARE_TYPE(ConvolveCubeData);

    RenderGraphResourceViewID mCubeTextureSRV;
    RenderGraphResourceID mConvolvedCubeTexture;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ConvolveCubeData& AddConvolveSkyCubePass(RenderGraph& inRenderGraph, Device& inDevice,
    const SkyCubeData& inSkyCubeData
);



////////////////////////////////////////
/// Skinning Compute Pass
////////////////////////////////////////
struct SkinningData
{
    RTTI_DECLARE_TYPE(SkinningData);
};

const SkinningData& AddSkinningPass(RenderGraph& inRenderGraph, Device& inDevice,
    const Scene& inScene
);



////////////////////////////////////////
/// GBuffer Render Pass
////////////////////////////////////////
struct GBufferOutput
{
    RenderGraphResourceID mDepthTexture;
    RenderGraphResourceID mRenderTexture;
    RenderGraphResourceID mVelocityTexture;
    RenderGraphResourceID mSelectionTexture;
};

struct GBufferData
{
    RTTI_DECLARE_TYPE(GBufferData);

    GBufferOutput mOutput;
    IRenderPass* mRenderPass = nullptr;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const GBufferData& AddGBufferPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene
);


////////////////////////////////////////
/// Meshlets Raster Render Pass
////////////////////////////////////////
const GBufferData& AddMeshletsRasterPass(RenderGraph& inRenderGraph, Device& inDevice,
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
    const GBufferOutput& inGBuffer,
    EDebugTexture inDebugTexture
);


////////////////////////////////////////
/// SSAO Render Pass
////////////////////////////////////////
struct SSAOTraceData
{
    RTTI_DECLARE_TYPE(SSAOTraceData);

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mDepthTexture;
    RenderGraphResourceViewID mGBufferTexture;
};


const SSAOTraceData& AddSSAOTracePass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferOutput& inGBuffer
);


////////////////////////////////////////
/// SSR Render Pass
////////////////////////////////////////
struct SSRTraceData
{
    RTTI_DECLARE_TYPE(SSRTraceData);

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mSceneTexture;
    RenderGraphResourceViewID mDepthTexture;
    RenderGraphResourceViewID mGBufferTexture;
};


const SSRTraceData& AddSSRTracePass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferOutput& inGBuffer,
    const RenderGraphResourceID inSceneTexture
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
    ComPtr<ID3D12PipelineState> mPipeline;
};

const GrassData& AddGrassRenderPass(RenderGraph& inGraph, Device& inDevice,
    const GBufferOutput& inGBuffer
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
};

const DownsampleData& AddDownsamplePass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID inSourceTexture
);



//////////////////////////////////////////
///// Tiled Light Culling Compute Pass
//////////////////////////////////////////
struct TiledLightCullingData
{
    RTTI_DECLARE_TYPE(TiledLightCullingData);

    RenderGraphResourceID mLightGridBuffer;
    RenderGraphResourceID mLightIndicesBuffer;
    TiledLightCullingRootConstants mRootConstants;
};

const TiledLightCullingData& AddTiledLightCullingPass(RenderGraph& inRenderGraph, Device& inDevice, const RayTracedScene& inScene);


//////////////////////////////////////////
///// Deferred Lighting Render Pass
//////////////////////////////////////////
struct LightingData
{
    RTTI_DECLARE_TYPE(LightingData);

    DDGIData        mDDGIData;
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mSkyCubeTextureSRV;
    RenderGraphResourceViewID mShadowMaskTextureSRV;
    RenderGraphResourceViewID mReflectionsTextureSRV;
    RenderGraphResourceViewID mGBufferDepthTextureSRV;
    RenderGraphResourceViewID mGBufferRenderTextureSRV;
    RenderGraphResourceViewID mIndirectDiffuseTextureSRV;
    RenderGraphResourceViewID mAmbientOcclusionTextureSRV;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const LightingData& AddLightingPass(RenderGraph& inRenderGraph, Device& inDevice, 
    const RayTracedScene& inScene,
    const GBufferOutput& inGBuffer, 
    const TiledLightCullingData& inLightData,
    RenderGraphResourceID inSkyCubeTexture,
    RenderGraphResourceID inShadowTexture, 
    RenderGraphResourceID inReflectionsTexture, 
    RenderGraphResourceID inAOTexture, 
    RenderGraphResourceID inIndirectDiffuseTexture
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
    const GBufferOutput& inGBuffer,
    RenderGraphResourceID inColorTexture,
    uint32_t inFrameCounter
);


////////////////////////////////////////
/// Depth of Field Render Pass
////////////////////////////////////////
struct DepthOfFieldData
{
    RTTI_DECLARE_TYPE(DepthOfFieldData);

    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mDepthTextureSRV;
    RenderGraphResourceViewID mInputTextureSRV;
};

const DepthOfFieldData& AddDepthOfFieldPass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID inInputTexture,
    RenderGraphResourceID mInDepthTexture
);



////////////////////////////////////////
/// Build Luminance Histogram Pass
////////////////////////////////////////
struct LuminanceHistogramData 
{
    RTTI_DECLARE_TYPE(LuminanceHistogramData);

    RenderGraphResourceID mHistogramBuffer;
    RenderGraphResourceViewID mInputTextureSRV;
};

const LuminanceHistogramData& AddLuminanceHistogramPass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID inInputTexture
);



////////////////////////////////////////
/// Final Compose Render Pass
////////////////////////////////////////
struct ComposeData
{
    RTTI_DECLARE_TYPE(ComposeData);

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
struct BloomBlurData
{
    RTTI_DECLARE_TYPE(BloomBlurData);

    uint32_t mToTextureMip = 0;
    uint32_t mFromTextureMip = 0;
    RenderGraphResourceViewID mToTextureUAV;
    RenderGraphResourceViewID mFromTextureSRV;
};


struct BloomPassData
{
    RTTI_DECLARE_TYPE(BloomPassData);

    RenderGraphResourceID mOutputTexture;
};


const BloomPassData& AddBloomPass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID inInputTexture
);


////////////////////////////////////////
/// Debug primitives graphics pass
////////////////////////////////////////
struct DebugPrimitivesData
{
    RTTI_DECLARE_TYPE(DebugPrimitivesData);

    uint32_t mLineVertexDataOffset = 0;
    uint32_t mTriangleVertexDataOffset = 0;

    RenderGraphResourceViewID mRenderTarget;
    RenderGraphResourceViewID mDepthTarget;

    ComPtr<ID3D12PipelineState> mPipeline;
};


const DebugPrimitivesData& AddDebugOverlayPass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID inRenderTarget,
    RenderGraphResourceID inDepthTarget
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
    Array<uint8_t> mIndexScratchBuffer;
    Array<uint8_t> mVertexScratchBuffer;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ImGuiData& AddImGuiPass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID inInputTexture,
    TextureID inBackBuffer
);

}
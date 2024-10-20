#pragma once

#include "RenderGraph.h"
#include "RayTracedScene.h"

namespace RK::DX12 {

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
struct GBufferData
{
    RTTI_DECLARE_TYPE(GBufferData);

    Entity mActiveEntity = Entity::Null;
    RenderGraphResourceID mDepthTexture;
    RenderGraphResourceID mRenderTexture;
    RenderGraphResourceID mVelocityTexture;
    RenderGraphResourceID mSelectionTexture;
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
    const GBufferData& inGBufferData,
    EDebugTexture inDebugTexture
);


////////////////////////////////////////
/// SSAO Render Pass
////////////////////////////////////////
struct SSAOTraceData
{
    RTTI_DECLARE_TYPE(SSAOTraceData);

    static inline int mSamples = 16;
    static inline float mBias = 0.025f;
    static inline float mRadius = 0.05f;
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mDepthTexture;
    RenderGraphResourceViewID mGBufferTexture;
};


const SSAOTraceData& AddSSAOTracePass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferData& inGBufferData
);


////////////////////////////////////////
/// SSR Render Pass
////////////////////////////////////////
struct SSRTraceData
{
    RTTI_DECLARE_TYPE(SSRTraceData);

    static inline int mSamples = 16;
    static inline float mBias = 0.025f;
    static inline float mRadius = 0.05f;
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mSceneTexture;
    RenderGraphResourceViewID mDepthTexture;
    RenderGraphResourceViewID mGBufferTexture;
};


const SSRTraceData& AddSSRTracePass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferData& inGBufferData,
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
    GrassRenderRootConstants mRenderConstants;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const GrassData& AddGrassRenderPass(RenderGraph& inGraph, Device& inDevice,
    const GBufferData& inGBufferData
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
    const GBufferData& inGBufferData, 
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

    static inline ComposeSettings mSettings = ComposeSettings {
        .mExposure = 1.0f,
        .mVignetteScale = 0.8f,
        .mVignetteBias =  0.2f,
        .mVignetteInner = 0.0f,
        .mVignetteOuter = 2.0f,
        .mBloomBlendFactor = 0.06f,
        .mChromaticAberrationStrength = 0.0f
    };

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
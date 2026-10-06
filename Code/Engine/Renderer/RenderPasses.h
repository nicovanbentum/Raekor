#pragma once

#include "RenderGraph.h"
#include "RayTracedScene.h"

namespace RK::DX12 {

struct RenderSettings
{
    static inline bool mDoFAutoFocus = true;
    static inline float mDoFAperture = 1.8f;
    static inline float mDoFFocusDistance = 5.0f;

    static inline int mGTAOSliceCount = 2;
    static inline int mGTAOStepCount = 8;
    static inline float mGTAORadius = 1.0f;
    static inline float mGTAOThickness = 0.25f;
    static inline float mGTAOPower = 1.0f;

    static inline int mSSRSamples = 16;
    static inline float mSSRBias = 0.025f;
    static inline float mSSRRadius = 0.05f;

    static inline float mGrassBend = 0.0f;
    static inline float mGrassTilt = 0.0f;
    static inline Vec2 mWindDirection = Vec2(0.0f, -1.0f);

    static inline float mEV100 = 15.0f;
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
    static inline uint32_t mPathTraceAlphaBounces = 4u;

    static inline bool mDDGIUseChebyshev = true;
    static inline bool mDDGIUseMultibounce = true;
    static inline float mDDGIDebugRadius = 0.25f;
    static inline IVec3 mDDGIDebugProbe = IVec3(0, 0, 0);
    static inline IVec3 mDDGIProbeCount = IVec3(16, 16, 16);
    static inline Vec3 mDDGIProbeSpacing = Vec3(6.4, 3.0, 2.8);
    static inline Vec3 mDDGICornerPosition = Vec3(-65, -1.4, -28.5);
    static inline bool mDDGIFollowCamera = false;
    static inline uint32_t mDDGICascadeCount = 1;
    static inline StaticArray<DDGIVolume, DDGI_MAX_CASCADES> mDDGIVolumes = {};

    static inline Entity mActiveEntity = Entity::Null;

    static float GetExposure() { return 1.0f / ( 1.2f * std::exp2(mEV100) ); }

    static int GetDDGIProbesPerCascade() { return mDDGIProbeCount.x * mDDGIProbeCount.y * mDDGIProbeCount.z; }
    static int GetDDGITotalProbeCount() { return GetDDGIProbesPerCascade() * mDDGICascadeCount; }

    static void UpdateDDGIVolumes(const Vec3& inCameraPosition);
    static uint32_t GetDDGIDebugProbeIndex();
    static DDGIData GetDDGIData();
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
    DEBUG_TEXTURE_GTAO,
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
    RenderGraphResourceViewID mBufferUAV;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ClearBufferData& AddClearBufferPass(RenderGraph& inRenderGraph, Device& inDevice,
    BufferID inTexture, 
    uint32_t inValue
);





////////////////////////////////////////
/// Transition Resource Pass
////////////////////////////////////////
struct TransitionResourceData
{
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
    RenderGraphResourceID mSkyCubeTexture;
};

const SkyCubeData& AddSkyCubePass(RenderGraph& inRenderGraph, Device& inDevice,
    const Scene& inScene 
);



////////////////////////////////////////
/// Convolve Irradiance Cubemap Cube Pass
////////////////////////////////////////
struct ConvolveCubeData
{
    RenderGraphResourceViewID mCubeTextureSRV;
    RenderGraphResourceID mConvolvedCubeTexture;
};

const ConvolveCubeData& AddConvolveSkyCubePass(RenderGraph& inRenderGraph, Device& inDevice,
    const Scene& inScene,
    const SkyCubeData& inSkyCubeData
);



////////////////////////////////////////
/// Integrate specular BRDF Pass
////////////////////////////////////////
struct IntegrateBrdfData
{
    bool isGenerated = false;
    RenderGraphResourceID outputTexture;
};

const IntegrateBrdfData& AddIntegrateBrdfPass(RenderGraph& inRenderGraph, Device& inDevice);



////////////////////////////////////////
/// Skinning Compute Pass
////////////////////////////////////////
struct SkinningData
{
    // all data is stored in Mesh
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
    GBufferOutput mOutput;
    IRenderPass* mRenderPass = nullptr;
    ComPtr<ID3D12PipelineState> mOpaquePipeline;
    ComPtr<ID3D12PipelineState> mMaskedPipeline;
};

const GBufferData& AddGBufferPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene
);


////////////////////////////////////////
/// Transparent Foward Render Pass
////////////////////////////////////////
struct DDGIOutput;

struct TransparentForwardData
{
    RenderGraphResourceViewID mOutputTextureRTV;
    RenderGraphResourceViewID mSelectionTextureRTV;
    RenderGraphResourceViewID mDepthTextureDSV;
    RenderGraphResourceViewID mBrdfLutTextureSRV;
    RenderGraphResourceViewID mSkyCubeTextureSRV;
    RenderGraphResourceViewID mDiffuseSkyCubeTextureSRV;
    RenderGraphResourceViewID mVolumesBufferSRV;
    RenderGraphResourceViewID mProbeDataBufferSRV;
    RenderGraphResourceViewID mProbesDepthTextureSRV;
    RenderGraphResourceViewID mProbesIrradianceTextureSRV;
    bool mUseRayTracedShadows = false;
    bool mUseIndirectDiffuse = false;
    ComPtr<ID3D12PipelineState> mBackFacePipeline;
    ComPtr<ID3D12PipelineState> mFrontFacePipeline;
};

const TransparentForwardData& AddTransparentForwardPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene,
    const GBufferOutput& inGBuffer,
    RenderGraphResourceID inRenderTarget,
    RenderGraphResourceID inBrdfLutTexture,
    RenderGraphResourceID inSkyCubeTexture,
    RenderGraphResourceID inDiffuseSkyCubeTexture,
    const DDGIOutput* inDDGI,
    bool inUseRayTracedShadows
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
    GBufferData mGBufferData;
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mInputTextureSRV;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const GBufferDebugData& AddGBufferDebugPass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferOutput& inGBuffer,
    EDebugTexture inDebugTexture
);



//////////////////////////////////////////
///// Shadow Map Pass
//////////////////////////////////////////
struct ShadowMapData
{
    RenderGraphResourceID mOutputTexture;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const ShadowMapData& AddShadowMapPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RayTracedScene& inScene
);


////////////////////////////////////////
/// GTAO Render Pass
////////////////////////////////////////
struct GTAOData
{
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mDepthTexture;
    RenderGraphResourceViewID mGBufferTexture;
};


const GTAOData& AddGTAOPass(RenderGraph& inRenderGraph, Device& inDevice,
    const GBufferOutput& inGBuffer
);


////////////////////////////////////////
/// SSR Render Pass
////////////////////////////////////////
struct SSRTraceData
{
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
    RenderGraphResourceID mGlobalAtomicBuffer;
    RenderGraphResourceViewID mSourceTextureUAV;
    RenderGraphResourceViewID mSourceTextureMipsUAVs[12];
};

const DownsampleData& AddDownsamplePass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID inSourceTexture,
    const char* inName
);



//////////////////////////////////////////
///// Tiled Light Culling Compute Pass
//////////////////////////////////////////
struct TiledLightCullingData
{
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
    DDGIData mDDGIData;
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mBrdfLutTextureSRV;
    RenderGraphResourceViewID mSkyCubeTextureSRV;
    RenderGraphResourceViewID mDiffuseSkyCubeTextureSRV;
    RenderGraphResourceViewID mShadowMaskTextureSRV;
    RenderGraphResourceViewID mReflectionsTextureSRV;
    RenderGraphResourceViewID mGBufferDepthTextureSRV;
    RenderGraphResourceViewID mGBufferRenderTextureSRV;
    RenderGraphResourceViewID mIndirectDiffuseTextureSRV;
    RenderGraphResourceViewID mAmbientOcclusionTextureSRV;
    bool mUseReflectionsTexture = false;
    bool mUseIndirectDiffuseTexture = false;
    ComPtr<ID3D12PipelineState> mPipeline;
};

const LightingData& AddLightingPass(RenderGraph& inRenderGraph, Device& inDevice, 
    const RayTracedScene& inScene,
    const GBufferOutput& inGBuffer, 
    const TiledLightCullingData& inLightData,
    RenderGraphResourceID inBrdfLutTexture,
    RenderGraphResourceID inSkyCubeTexture,
    RenderGraphResourceID inDiffuseCubeTexture,
    RenderGraphResourceID inShadowTexture, 
    RenderGraphResourceID inReflectionsTexture, 
    RenderGraphResourceID inAOTexture, 
    RenderGraphResourceID inIndirectDiffuseTexture,
    bool inUseReflectionsTexture,
    bool inUseIndirectDiffuseTexture
);



//////////////////////////////////////////
///// TAA Resolve Render Pass
//////////////////////////////////////////
struct TAAResolveData
{
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
    RenderGraphResourceID inColorTexture
);


struct CopyTextureGraphicsData
{
    ComPtr<ID3D12PipelineState> mPipeline;
    RenderGraphResourceViewID mSrcTextureSRV;
    RenderGraphResourceViewID mDstTextureRTV;
};


////////////////////////////////////////
/// Depth of Field Render Pass
////////////////////////////////////////
struct DepthOfFieldData
{
    RenderGraphResourceID mOutputTexture;
    RenderGraphResourceViewID mInputTextureSRV;
    RenderGraphResourceViewID mDepthTextureSRV;
    RenderGraphResourceViewID mBlurTextureSRV;
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
    uint32_t mToTextureMip = 0;
    uint32_t mFromTextureMip = 0;
    RenderGraphResourceViewID mToTextureUAV;
    RenderGraphResourceViewID mFromTextureSRV;
};


struct BloomPassData
{
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



struct SDFUIData
{
    RenderGraphResourceID mDrawCommandBuffer;
    RenderGraphResourceID mDrawCommandHeaderBuffer;
    RenderGraphResourceViewID mDrawCommandBufferSRV;
    RenderGraphResourceViewID mDrawCommandHeaderBufferSRV;
    ComPtr<ID3D12PipelineState> mPipeline;
};


const SDFUIData& AddSDFUIPass(RenderGraph& inRenderGraph, Device& inDevice, 
    RenderGraphResourceID inRenderTarget);



////////////////////////////////////////
/// Pre-ImGui Pass
////////////////////////////////////////
struct PreImGuiData
{
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
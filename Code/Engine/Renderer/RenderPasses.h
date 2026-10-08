#pragma once

#include "RenderGraph.h"
#include "RenderWorld.h"

namespace RK::DX12 {

struct RenderSettings
{
    static RK_API bool mDoFAutoFocus;
    static RK_API float mDoFAperture;
    static RK_API float mDoFFocusDistance;

    static RK_API int mGTAOSliceCount;
    static RK_API int mGTAOStepCount;
    static RK_API float mGTAORadius;
    static RK_API float mGTAOThickness;
    static RK_API float mGTAOPower;

    static RK_API int mSSRSamples;
    static RK_API float mSSRBias;
    static RK_API float mSSRRadius;

    static RK_API float mGrassBend;
    static RK_API float mGrassTilt;
    static RK_API Vec2 mWindDirection;

    static RK_API float mEV100;
    static RK_API float mVignetteScale;
    static RK_API float mVignetteBias;
    static RK_API float mVignetteInner;
    static RK_API float mVignetteOuter;
    static RK_API float mBloomBlendFactor;
    static RK_API float mChromaticAberrationStrength;

    static RK_API float mRTAORadius;
    static RK_API float mRTAOPower;
    static RK_API float mRTAONormalBias;
    static RK_API uint32_t mRTAOSampleCount;

    static RK_API bool mPathTraceReset;
    static RK_API uint32_t mPathTraceBounces;

    static RK_API bool mDDGIUseChebyshev;
    static RK_API bool mDDGIUseMultibounce;
    static RK_API float mDDGIDebugRadius;
    static RK_API IVec3 mDDGIDebugProbe;
    static RK_API IVec3 mDDGIProbeCount;
    static RK_API Vec3 mDDGIProbeSpacing;
    static RK_API Vec3 mDDGICornerPosition;
    static RK_API bool mDDGIFollowCamera;
    static RK_API uint32_t mDDGICascadeCount;
    static RK_API float mDDGIIrradianceHysteresis;
    static RK_API bool mDDGIRelocateAllProbes;
    static RK_API StaticArray<DDGIVolume, DDGI_MAX_CASCADES> mDDGIVolumes;

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


enum EWireframeMode
{
    WIREFRAME_MODE_OFF = 0,
    WIREFRAME_MODE_OVERLAY,
    WIREFRAME_MODE_XRAY,
    WIREFRAME_MODE_COUNT
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
    const RenderWorld& inWorld
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
    const RenderWorld& inWorld,
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
};

const SkinningData& AddSkinningPass(RenderGraph& inRenderGraph, Device& inDevice,
    const RenderWorld& inWorld
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
    const RenderWorld& inWorld
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
    const RenderWorld& inWorld,
    const GBufferOutput& inGBuffer,
    RenderGraphResourceID inRenderTarget,
    RenderGraphResourceID inBrdfLutTexture,
    RenderGraphResourceID inSkyCubeTexture,
    RenderGraphResourceID inDiffuseSkyCubeTexture,
    const DDGIOutput* inDDGI,
    bool inUseRayTracedShadows
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

const TiledLightCullingData& AddTiledLightCullingPass(RenderGraph& inRenderGraph, Device& inDevice);


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


////////////////////////////////////////
/// Wireframe debug graphics pass
////////////////////////////////////////
struct WireframeData
{
    RenderGraphResourceViewID mRenderTarget;
    RenderGraphResourceViewID mDepthTarget;

    ComPtr<ID3D12PipelineState> mPipeline;
};


const WireframeData& AddWireframePass(RenderGraph& inRenderGraph, Device& inDevice, const RenderWorld& inWorld,
    RenderGraphResourceID inRenderTarget,
    RenderGraphResourceID inDepthTarget,
    bool inDepthTest
);



struct SDFUIData
{
    static constexpr uint32_t cMaxPrimitives = 64 * 1024;

    RenderGraphResourceID mPrimitivesBuffer;
    RenderGraphResourceViewID mPrimitivesBufferSRV;
    ComPtr<ID3D12PipelineState> mPipeline;
};


const SDFUIData& AddSDFUIPass(RenderGraph& inRenderGraph, Device& inDevice,
    RenderGraphResourceID inRenderTarget,
    const TextureID& inFontAtlas);



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

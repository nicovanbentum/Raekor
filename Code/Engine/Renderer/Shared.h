#ifndef SHARED_H
#define SHARED_H

#ifdef __cplusplus

    #define IF_CPP(code) code
    #define IF_CPP_ELSE(cpp, other) cpp

    #include "pch.h"

    class Device;

    using uint = uint32_t;
    using uint2 = glm::uvec2;
    using uint3 = glm::uvec3;
    using uint4 = glm::uvec4;

    using int2 = glm::ivec2;
    using int3 = glm::ivec3;
    using int4 = glm::ivec4;

    using float2 = glm::vec2;
    using float3 = glm::vec3;
    using float4 = glm::vec4;

    using float3x3 = glm::mat3;
    using float4x4 = glm::mat4;

    using Texture2D = uint;

#else

    #define IF_CPP(code)
    #define IF_CPP_ELSE(cpp, other) other

#endif

#define BRDF_LUT_SIZE 512
#define DDGI_TRACE_SIZE 64                  // Thread group size for the ray trace shader. Sorry AMD, I'm running a 3080
#define DDGI_DEPTH_TEXELS 16                // Depth is stored as 16x16 FORMAT_R32F texels
#define DDGI_DEPTH_TEXELS_NO_BORDER 14      // Depth is stored as 16x16 FORMAT_R32F texels
#define DDGI_IRRADIANCE_TEXELS 8            // Irradiance is stored as 6x6 FORMAT_R11G11B10F texels with a 1 pixel border
#define DDGI_IRRADIANCE_TEXELS_NO_BORDER 6  // Irradiance is stored as 6x6 FORMAT_R11G11B10F texels with a 1 pixel border
#define DDGI_PROBES_PER_ROW 40              // Number of probes per row for the final probe texture
#define DDGI_RAYS_PER_PROBE 192             // Basically wave size * rays per wave
#define DDGI_FIXED_RAYS 32
#define DDGI_FIXED_RAYS_BACKFACE_THRESHOLD 21
#define DDGI_RELOCATION_FRAMES 16
#define DDGI_MAX_CASCADES 4
#define DOF_TILE_SIZE 8

#define DENOISE_SPATIAL_ITERATIONS 3

#define LIGHT_CULL_TILE_SIZE 16 // Light culling uses 16x16 pixel screen tiles
#define LIGHT_CULL_MAX_LIGHTS 1024 // Max lights per tile for light culling

#define BINDLESS_BLUE_NOISE_TEXTURE_INDEX 5

struct LineVertex
{
    float4 mPosition;
    float4 mColor;
};


struct RTGeometry
{
    uint     mEntity;
    uint     mIndexBuffer;
    uint     mVertexBuffer;
    uint     mMaterialIndex;
    float4x4 mWorldTransform;
    float4x4 mPrevWorldTransform;
};


struct RTMaterial
{
    float  mMetallic;
    float  mRoughness;
    uint   mAlbedoTexture;
    uint   mNormalsTexture;
    uint   mEmissiveTexture;
    uint   mMetallicTexture;
    uint   mRoughnessTexture;
    float  mAlphaCutoff;
    float4 mAlbedo;
    float4 mEmissive;
};


enum RTELightType
{
    RT_LIGHT_TYPE_NONE = 0,
    RT_LIGHT_TYPE_SPOT,
    RT_LIGHT_TYPE_POINT,
    RT_LIGHT_TYPE_COUNT,
};


struct RTLight
{
    uint mType;
    float3 mDirection;
    float4 mPosition;
    float4 mColor;
    float4 mAttributes;
};


struct RTVertex
{
    float3 mPos;
    float2 mTexCoord;
    float3 mNormal;
    float3 mTangent;
};


struct GlobalConstants
{
    uint mBlueNoiseTexture;
    uint mSkyCubeTexture;
    uint mConvolvedSkyCubeTexture;
    uint mDebugLinesVertexBuffer;
    uint mDebugLinesIndirectArgsBuffer;
};


struct FrameConstants
{
    float     mTime;
    float     mDeltaTime;
    float     mSunConeAngle;
    uint      mTLAS;

    uint      mFrameCounter;
    uint      mDebugLinesVertexBuffer;
    uint      mDebugLinesIndirectArgsBuffer;
    uint      mLightsBuffer;
    
    uint      mInstancesBuffer;
    uint      mMaterialsBuffer;
    uint2     mViewportSize;
    
    float     mExposure;
    uint      mNrOfLights;
    uint      mShadowTLAS;
    uint      mPad0;

    float2    mJitter;
    float2    mPrevJitter;
    
    float4    mSunColor;
    float4    mSunDirection;
    float4    mCameraPosition;
    
    float4x4  mViewMatrix;
    float4x4  mInvViewMatrix;
    float4x4  mProjectionMatrix;
    float4x4  mInvProjectionMatrix;
    float4x4  mViewProjectionMatrix;
    float4x4  mInvViewProjectionMatrix;
    float4x4  mPrevViewProjectionMatrix;
};

struct DebugPrimitivesRootConstants
{
    uint mBufferOffset;
};


struct GPULinesRootConstants
{
    uint mVertexBuffer;
};


struct DDGIVolume
{
    float3 mCornerPosition;
    uint   mProbeOffset;
    float3 mProbeSpacing;
    uint   mPad0;
    int3   mOriginCell;
    uint   mPad1;
    int3   mScrollOffset;
    uint   mPad2;
};


struct DDGIData
{
    int3   mProbeCount;
    float  mProbeRadius;
    uint   mUseChebyshev;
    uint   mCascadeCount;
    uint   mVolumesBuffer;
    uint   mProbesDataBuffer;
    uint   mRaysDepthTexture;
    uint   mProbesDepthTexture;
    uint   mRaysIrradianceTexture;
    uint   mProbesIrradianceTexture;
};


struct ProbeData
{
    uint inactive;
    float3 offset;
    int3 cell;
    uint reset;
    uint age;
};


struct ClearTextureRootConstants
{
    float4 mClearValue;
    uint mTexture;
};


struct CopyTextureConstants
{
    uint mSrcTexture;
};


struct ClearBufferRootConstants
{
    uint mClearValue;
    uint mBuffer;
};


struct GTAORootConstants
{
    uint  mOutputTexture;
    uint  mDepthTexture;
    uint  mGBufferTexture;
    uint  mSliceCount;
    uint  mStepCount;
    float mRadius;
    float mThickness;
    float mPower;
    uint2 mDispatchSize;
};


struct SSRTraceRootConstants
{
    uint  mOutputTexture;
    uint  mSceneTexture;
    uint  mDepthTexture;
    uint  mGBufferTexture;
    float mRadius;
    float mBias;
    uint  mSamples;
    uint  mPad1;
    uint2 mDispatchSize;
};

struct ConvolveCubeRootConstants
{
    uint mCubeTexture;
    uint mConvolvedCubeTexture;
};


struct SkyCubeRootConstants
{
    uint mSkyCubeTexture;
    float3 mSunLightDirection;
    float4 mSunLightColor;
};


struct IntegrateBrdfConstants
{
    uint mOutputTexture;
};


struct SkinningRootConstants
{
    uint mBoneIndicesBuffer;
    uint mBoneWeightsBuffer;
    uint mMeshVertexBuffer;
    uint mSkinnedVertexBuffer;
    uint mBoneTransformsBuffer;
    uint mDispatchSize;
};


struct GbufferRootConstants
{
    uint     mEntity;
    uint     mInstanceIndex;
};


struct ShadowMapRootConstants
{
    uint     mInstanceIndex;
    uint     mPad0;
    float4x4 mViewProjMatrix;
};


struct GbufferDebugRootConstants
{
    uint     mTexture;
    float    mFarPlane;
    float    mNearPlane;
};


struct ShadowMaskRootConstants
{
    uint  mShadowMaskTexture;
    uint  mGbufferDepthTexture;
    uint  mGbufferRenderTexture;
    uint  mPad0;
    uint2 mDispatchSize;
};


struct AmbientOcclusionParams
{
    float mRadius;
    float mPower;
    float mNormalBias;
    uint  mSampleCount;
};


struct AmbientOcclusionRootConstants
{
    uint  mAOmaskTexture;
    uint  mGbufferDepthTexture;
    uint  mGbufferRenderTexture;
    uint  mPad0;
    uint2 mDispatchSize;
    uint2 mPad1;
    AmbientOcclusionParams mParams;
};


struct ReflectionsRootConstants
{
    uint  mResultTexture;
    uint  mSkyCubeTexture;
    uint  mDiffuseSkyCubeTexture;
    uint  mUseDDGI;
    uint  mGbufferDepthTexture;
    uint  mGbufferRenderTexture;
    uint2 mDispatchSize;
    DDGIData mDDGIData;
};


struct PathTraceRootConstants
{
    uint  mReset;
    uint  mBounces;
    uint  mResultTexture;
    uint  mAccumulationTexture;
    uint  mSelectionTexture;
    uint  mDepthTexture;
    uint  mSkyCubeTexture;
    uint  mGBufferTexture;
    uint2 mDispatchSize;
};



struct DenoiseRootConstants
{
    uint  mInputTexture;
    uint  mHistoryTexture;
    uint  mOutputTexture;
    uint  mDepthTexture;
    uint  mGBufferTexture;
    uint  mVelocityTexture;
    uint  mStepSize;
    uint  mPad0;
    uint2 mDispatchSize;
};



struct SpdRootConstants
{
    uint   mNrOfMips;
    uint   mNrOfWorkGroups;
    uint   mGlobalAtomicBuffer;
    uint   mTextureMip0;
    uint   mTextureMip1;
    uint   mTextureMip2;
    uint   mTextureMip3;
    uint   mTextureMip4;
    uint   mTextureMip5;
    uint   mTextureMip6;
    uint   mTextureMip7;
    uint   mTextureMip8;
    uint   mTextureMip9;
    uint   mTextureMip10;
    uint   mTextureMip11;
    uint   mTextureMip12;
    uint   mTextureMip13;
    uint   mPad0;
    uint2  mWorkGroupOffset;
};



struct TiledLightCullingRootConstants
{
    uint  mLightGridBuffer;
    uint  mLightIndicesBuffer;
    uint2 mFullResSize;
    uint2 mDispatchSize;
};



struct TransparentForwardConstants
{
    uint     mEntity;
    uint     mInstanceIndex;
    uint     mBrdfLutTexture;
    uint     mSkyCubeTexture;
    uint     mDiffuseSkyCubeTexture;
    uint     mUseRayTracedShadows;
    uint     mUseIndirectDiffuse;
    uint     mPad0;
    DDGIData mDDGIData;
};



struct LightingRootConstants
{
    uint  mBrdfLutTexture;
    uint  mUseReflectionsTexture;
    uint  mUseIndirectDiffuseTexture;
    uint  mPad0;
    uint  mSkyCubeTexture;
    uint  mDiffuseSkyCubeTexture;
    uint  mShadowMaskTexture;
    uint  mReflectionsTexture;
    uint  mGbufferDepthTexture;
    uint  mGbufferRenderTexture;
    uint  mIndirectDiffuseTexture;
    uint  mAmbientOcclusionTexture;
    TiledLightCullingRootConstants mLights;
};


struct HeightFogRootConstants
{
    uint mGbufferDepthTexture;
    uint mGbufferRenderTexture;
};


struct GrassRenderRootConstants
{
    float  mBend;
    float  mTilt;
    float2 mWindDirection;
};


struct ProbeTraceRootConstants
{
    uint     mDebugProbeIndex;
    uint     mSkyCubeTexture;
    uint     mConvolvedSkyCubeTexture;
    uint     mPad0;
    float4x4 mRandomRotationMatrix;
    DDGIData mDDGIData;
};


struct ProbeUpdateRootConstants
{
    float4x4 mRandomRotationMatrix;
    DDGIData mDDGIData;
    float    mIrradianceHysteresis;
    uint     mRelocateAllProbes;
};


struct ProbeSampleRootConstants
{
    DDGIData mDDGIData;
    uint mOutputTexture;
    uint mDepthTexture;
    uint mGBufferTexture;
    uint mPad0;
    uint2 mDispatchSize;
};


struct TAAResolveConstants
{
    uint2 mRenderSize;
    float2 mRenderSizeRcp;
    uint mColorTexture;
    uint mDepthTexture;
    uint mHistoryTexture;
    uint mVelocityTexture;
};


struct BloomRootConstants
{
    uint mSrcTexture;
    uint mSrcMipLevel;
    uint mDstTexture;
    uint mDstMipLevel;
    uint2 mDispatchSize;
    float2 mSrcSizeRcp;
};


struct ComposeSettings
{
    float mExposure;
    float mVignetteScale;
    float mVignetteBias;
    float mVignetteInner;
    float mVignetteOuter;
    float mBloomBlendFactor;
    float mChromaticAberrationStrength;
};

struct ComposeRootConstants
{
    uint mBloomTexture;
    uint mInputTexture;
    uint mPad0;
    uint mPad1;
    ComposeSettings mSettings;
};


struct DepthOfFieldRootConstants
{
    uint  mInputTexture;
    uint  mDepthTexture;
    uint  mHalfResTexture;
    uint  mTileTexture;
    uint  mBlurTexture;
    uint  mOutputTexture;
    uint  mAutoFocus;
    float mFocusDistance;
    float mFocalLength;
    float mLensCoefficient;
    float mMaxCoC;
    float mNearPlane;
    float mFarPlane;
    uint  mPad0;
    uint2 mDispatchSize;
};


struct ImGuiRootConstants
{
    float4x4 mProjection;
    uint mBindlessTextureIndex;
};

enum EUIPrimitiveType
{
    UI_PRIMITIVE_RECT,
    UI_PRIMITIVE_CIRCLE,
    UI_PRIMITIVE_GLYPH,
};

struct UIPrimitive
{
    float4 mColor;
    float4 mRect;
    float4 mUVRect;
    float  mRadius;
    float  mThickness;
    float  mSoftness;
    uint   mType;
};

struct SDFUIRootConstants
{
    uint   mPrimitivesBuffer;
    uint   mFontAtlasTexture;
    float  mFontDistanceRange;
    float  mFontAtlasPixelHeight;
    float2 mInvRenderSize;
};


#endif // SHARED_H

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

#else

    #define IF_CPP(code)
    #define IF_CPP_ELSE(cpp, other) other

#endif

#define DDGI_TRACE_SIZE 64                  // Thread group size for the ray trace shader. Sorry AMD, I'm running a 3080
#define DDGI_DEPTH_TEXELS 16                // Depth is stored as 16x16 FORMAT_R32F texels
#define DDGI_DEPTH_TEXELS_NO_BORDER 14      // Depth is stored as 16x16 FORMAT_R32F texels
#define DDGI_IRRADIANCE_TEXELS 8            // Irradiance is stored as 6x6 FORMAT_R11G11B10F texels with a 1 pixel border
#define DDGI_IRRADIANCE_TEXELS_NO_BORDER 6  // Irradiance is stored as 6x6 FORMAT_R11G11B10F texels with a 1 pixel border
#define DDGI_PROBES_PER_ROW 40              // Number of probes per row for the final probe texture
#define DDGI_RAYS_PER_PROBE 256             // Basically wave size * rays per wave

#define RT_SHADOWS_GROUP_DIM 16     // RT shadows divides the screen up in 16x16 pixel tiles
#define RT_SHADOWS_PACKED_DIM_X 8   // RT shadows stores every 8x4 pixels ray results as a 32bit mask
#define RT_SHADOWS_PACKED_DIM_Y 4   // RT shadows stores every 8x4 pixels ray results as a 32bit mask

#define LIGHT_CULL_TILE_SIZE 16 // Light culling uses 16x16 pixel screen tiles
#define LIGHT_CULL_MAX_LIGHTS 1024 // Max lights per tile for light culling

#define BINDLESS_BLUE_NOISE_TEXTURE_INDEX 2
#define BINDLESS_SKY_CUBE_TEXTURE_INDEX 3
#define BINDLESS_SKY_CUBE_DIFFUSE_TEXTURE_INDEX 4

struct LineVertex
{
    float4 mPosition;
    float4 mColor;
};


struct RTGeometry
{
    uint     mIndexBuffer;
    uint     mVertexBuffer;
    uint     mMaterialIndex;
    float4x4 mLocalToWorldTransform;
    float4x4 mInvLocalToWorldTransform;
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
    uint   mPad0;
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
    float     mPad0;
    uint      mFrameIndex;
    uint      mFrameCounter;
    uint      mDebugLinesVertexBuffer;
    uint      mDebugLinesIndirectArgsBuffer;
    float2    mJitter;
    float2    mPrevJitter;
    float4    mSunColor;
    float4    mSunDirection;
    float4    mCameraPosition;
    float4x4  mViewMatrix;
    float4x4  mProjectionMatrix;
    float4x4  mViewProjectionMatrix;
    float4x4  mInvViewProjectionMatrix;
    float4x4  mPrevViewProjectionMatrix;
};


struct DebugPrimitivesRootConstants
{
    uint mBufferOffset;
};


struct DDGIData
{
    uint   mRaysDepthTexture;
    uint   mRaysIrradianceTexture;
    uint   mProbesDepthTexture;
    uint   mProbesIrradianceTexture;
    int3   mProbeCount;
    float  mProbeRadius;
    float3 mProbeSpacing;
    uint   pad1;
    float3 mCornerPosition;
    uint   pad2;
};


struct ClearTextureRootConstants
{
    float4 mClearValue;
    uint mTexture;
};


struct ClearBufferRootConstants
{
    uint mClearValue;
    uint mBuffer;
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
    uint     mInstancesBuffer;
    uint     mMaterialsBuffer;
    uint     mInstanceIndex;
    uint     mEntity;
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
    uint  mTLAS;
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
    AmbientOcclusionParams mParams;
    uint  mAOmaskTexture;
    uint  mGbufferDepthTexture;
    uint  mGbufferRenderTexture;
    uint  mTLAS;
    uint2 mDispatchSize;
};


struct ReflectionsRootConstants
{
    uint  mTLAS;
    uint  mInstancesBuffer;
    uint  mMaterialsBuffer;
    uint  mResultTexture;
    uint  mGbufferDepthTexture;
    uint  mGbufferRenderTexture;
    uint2 mDispatchSize;
};


struct PathTraceRootConstants
{
    uint  mTLAS;
    uint  mBounces;
    uint  mLightsBuffer;
    uint  mInstancesBuffer;
    uint  mMaterialsBuffer;
    uint  mResultTexture;
    uint  mAccumulationTexture;
    uint  mPad0;
    uint2 mDispatchSize;
    uint  mReset;
    uint mLightsCount;
};



struct ShadowsClearRootConstants
{
    uint mTilesBuffer;
    uint mDispatchBuffer;
};


struct ShadowsClassifyRootConstants
{
    uint mShadowMaskTexture;
    uint mTilesBuffer;
    uint mDispatchBuffer;
    uint2 mDispatchSize;
};


struct ShadowsDenoiseRootConstants
{
    uint mResultTexture;
    uint mHistoryTexture;
    uint mDepthTexture;
    uint mGBufferTexture;
    uint mVelocityTexture;
    uint mShadowMaskTexture;
    uint mTilesBuffer;
    uint mPad0;
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
    uint2  mWorkGroupOffset;
};



struct TiledLightCullingRootConstants
{
    uint  mLightsCount;
    uint  mLightsBuffer;
    uint  mLightGridBuffer;
    uint  mLightIndicesBuffer;
    uint2 mFullResSize;
    uint2 mDispatchSize;
};



struct LightingRootConstants
{
    uint  mLightsCount;
    uint  mLightsBuffer;
    uint  mShadowMaskTexture;
    uint  mReflectionsTexture;
    uint  mGbufferDepthTexture;
    uint  mGbufferRenderTexture;
    uint  mIndirectDiffuseTexture;
    uint  mAmbientOcclusionTexture;
    uint  mPad0;
    uint  mPad1;
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
    uint     mInstancesBuffer;
    uint     mMaterialsBuffer;
    uint     mTLAS;
    uint     mDebugProbeIndex;
    uint     mLightsBuffer;
    uint     mLightsCount;
    uint     mPad0;
    uint     mPad1;
    float4x4 mRandomRotationMatrix;
    DDGIData mDDGIData;
};


struct ProbeUpdateRootConstants
{
    float4x4 mRandomRotationMatrix;
    DDGIData mDDGIData;
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


struct ComposeRootConstants
{
    uint mBloomTexture;
    uint mInputTexture;
    float mExposure;
    float mBloomBlendFactor;
    float mChromaticAberrationStrength;
};


struct DepthOfFieldRootConstants
{
    uint mDepthTexture;
    uint mInputTexture;
    uint mOutputTexture;
    uint mPad1;
    float mFarPlane;
    float mNearPlane;
    float mFocusPoint;
    float mFocusScale;
    uint2 mDispatchSize;
};


struct ImGuiRootConstants
{
    float4x4 mProjection;
    uint mBindlessTextureIndex;
};



#endif // SHARED_H

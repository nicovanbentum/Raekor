#ifndef SHARED_H
#define SHARED_H

#ifdef __cplusplus
    #include "pch.h"
    using uint      = uint32_t;
    using uint2     = glm::uvec2;
    using uint3     = glm::uvec3;
    using uint4     = glm::uvec4;
    
    using int2      = glm::ivec2;
    using int3      = glm::ivec3;
    using int4      = glm::ivec4;
    
    using float2    = glm::vec2;
    using float3    = glm::vec3;
    using float4    = glm::vec4;
    
    using float3x3  = glm::mat3;
    using float4x4  = glm::mat4;

#define OUT_PARAM
#define STATIC_ASSERT(cond) static_assert(cond);

#else

#define OUT_PARAM out
#define STATIC_ASSERT(cond) 

#endif

// NEEDS TO MATCH DXUtil.h , its 256 - 2 SRV root descriptors (which are 2 DWORDs each)
#define MAX_ROOT_CONSTANTS_SIZE 232

#define DDGI_WAVE_SIZE 64                        // Thread group size for the ray trace shader. Sorry AMD, I'm running a 3080
#define DDGI_RAYS_PER_WAVE 3                     // This is the number of rays per probe (192) divided by the thread group size (64)
#define DDGI_DEPTH_TEXELS 16                     // Depth is stored as 16x16 FORMAT_R32F texels
#define DDGI_DEPTH_TEXELS_NO_BORDER 14           // Depth is stored as 16x16 FORMAT_R32F texels
#define DDGI_IRRADIANCE_TEXELS 8                 // Irradiance is stored as 6x6 FORMAT_R11G11B10F texels with a 1 pixel border
#define DDGI_IRRADIANCE_TEXELS_NO_BORDER 6       // Irradiance is stored as 6x6 FORMAT_R11G11B10F texels with a 1 pixel border
#define DDGI_PROBES_PER_ROW 40                   // Number of probes per row for the final probe texture
#define DDGI_RAYS_PER_PROBE 192                  // Basically wave size * rays per wave

#define BINDLESS_BLUE_NOISE_TEXTURE_INDEX 1

struct LineVertex {
    float4 mPosition;
    float4 mColor;
};


struct RTGeometry {
    uint     mIndexBuffer;
    uint     mVertexBuffer;
    uint     mMaterialIndex;
    float4x4 mLocalToWorldTransform;
};


struct RTMaterial {
    float  mMetallic;
    float  mRoughness;
    uint   mAlbedoTexture;
    uint   mNormalsTexture;
    uint   mMetalRoughTexture;
    float4 mAlbedo;
    float4 mEmissive;
};


struct RTVertex {
    float3 mPos;
    float2 mTexCoord;
    float3 mNormal;
    float3 mTangent;
};
  

struct FrameConstants {
    float     mTime;
    float     mDeltaTime;
    uint      mFrameIndex;
    uint      mFrameCounter;
    uint      mDebugLinesVertexBuffer;
    uint      mDebugLinesIndirectArgsBuffer;
    float4    mSunDirection;
    float4    mCameraPosition;
    float4x4  mViewMatrix;
    float4x4  mProjectionMatrix;
    float4x4  mViewProjectionMatrix;
    float4x4  mInvViewProjectionMatrix;
    float4x4  mPrevViewProjectionMatrix;
};


struct DDGIData {
    uint   mRaysDepthTexture;
    uint   mRaysIrradianceTexture;
    uint   mProbesDepthTexture;
    uint   mProbesIrradianceTexture;
    int3   mProbeCount;
    uint   pad0;
    float3 mProbeSpacing;
    uint   pad1;
    float3 mCornerPosition;
    uint   pad2;
};
STATIC_ASSERT(sizeof(DDGIData) < MAX_ROOT_CONSTANTS_SIZE);


struct GbufferRootConstants {
    uint     mVertexBuffer;
    uint     mAlbedoTexture;
    uint     mNormalTexture;
    uint     mMetalRoughTexture;
    float4	 mAlbedo;
    float    mRoughness;
    float    mMetallic;
    float    pad0;
    float    pad1;
    float4x4 mWorldTransform;
    float4x4 mInvWorldTransform;
}; 
STATIC_ASSERT(sizeof(GbufferRootConstants) < MAX_ROOT_CONSTANTS_SIZE);


struct GbufferDebugRootConstants {
    uint     mTexture;
    float    mFarPlane;
    float    mNearPlane;
    float    pad0;
};
STATIC_ASSERT(sizeof(GbufferDebugRootConstants) < MAX_ROOT_CONSTANTS_SIZE);


struct ShadowMaskRootConstants {
    uint  mGbufferRenderTexture;
    uint  mGbufferDepthTexture;
    uint  mShadowMaskTexture;
    uint  mTLAS;
    uint2 mDispatchSize;
}; 
STATIC_ASSERT(sizeof(ShadowMaskRootConstants) < MAX_ROOT_CONSTANTS_SIZE);


struct AmbientOcclusionParams {
    float mRadius;
    float mIntensity;
    float mNormalBias;
    uint  mSampleCount;
};


struct AmbientOcclusionRootConstants {
    AmbientOcclusionParams mParams;
    uint  mGbufferRenderTexture;
    uint  mGbufferDepthTexture;
    uint  mAOmaskTexture;
    uint  mTLAS;
    uint2 mDispatchSize;
};
STATIC_ASSERT(sizeof(AmbientOcclusionRootConstants) < MAX_ROOT_CONSTANTS_SIZE);


struct ReflectionsRootConstants {
    uint  mGbufferRenderTexture;
    uint  mGbufferDepthTexture;
    uint  mShadowMaskTexture;
    uint  mTLAS;
    uint  mInstancesBuffer;
    uint  mMaterialsBuffer;
    uint2 mDispatchSize;
}; 
STATIC_ASSERT(sizeof(ReflectionsRootConstants) < MAX_ROOT_CONSTANTS_SIZE);


struct IndirectGIRootConstants {
    uint  mGbufferRenderTexture;
    uint  mGbufferDepthTexture;
    uint  mShadowMaskTexture;
    uint  mTLAS;
    uint  mInstancesBuffer;
    uint  mMaterialsBuffer;
    uint2 mDispatchSize;
};
STATIC_ASSERT(sizeof(IndirectGIRootConstants) < MAX_ROOT_CONSTANTS_SIZE);


struct SpdRootConstants {
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
STATIC_ASSERT(sizeof(SpdRootConstants) < MAX_ROOT_CONSTANTS_SIZE);


struct LightingRootConstants {
    uint  mShadowMaskTexture;
    uint  mReflectionsTexture;
    uint  mGbufferDepthTexture;
    uint  mGbufferRenderTexture;
    uint  mAmbientOcclusionTexture;
    uint  mIndirectDiffuseTexture;
    uint2 pad2;
    DDGIData mDDGIData;
}; 
STATIC_ASSERT(sizeof(LightingRootConstants) < MAX_ROOT_CONSTANTS_SIZE);


struct HeightFogRootConstants {
    uint mGbufferDepthTexture;
    uint mGbufferRenderTexture;
}; 
STATIC_ASSERT(sizeof(HeightFogRootConstants) < MAX_ROOT_CONSTANTS_SIZE);


struct GrassRenderRootConstants {
    float  mBend;
    float  mTilt;
    float2 mWindDirection;
};
STATIC_ASSERT(sizeof(GrassRenderRootConstants) < MAX_ROOT_CONSTANTS_SIZE);


struct ProbeTraceRootConstants {
    uint     mInstancesBuffer;
    uint     mMaterialsBuffer;
    uint     mTLAS;
    uint     mDebugProbeIndex;
    DDGIData mDDGIData;
    float3x3 mRandomRotationMatrix;
};
STATIC_ASSERT(sizeof(ProbeTraceRootConstants) < MAX_ROOT_CONSTANTS_SIZE);


struct ProbeUpdateRootConstants {
    DDGIData mDDGIData;
    float3x3 mRandomRotationMatrix;
};
STATIC_ASSERT(sizeof(ProbeUpdateRootConstants) < MAX_ROOT_CONSTANTS_SIZE);


struct ImGuiRootConstants {
    float4x4 mProjection;
    uint mBindlessTextureIndex;
};
STATIC_ASSERT(sizeof(ImGuiRootConstants) < MAX_ROOT_CONSTANTS_SIZE);


#endif // SHARED_H

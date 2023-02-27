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
#define ALIGN_16 __declspec(align(16))
#define STATIC_ASSERT(cond) static_assert(cond);

#else

#define OUT_PARAM out
#define ALIGN_16
#define STATIC_ASSERT(cond) 

#endif

#define MAX_ROOT_CONSTANTS_SIZE 128 // Following the Vulkan spec here, we need some space for root descriptors

/* DDGI DEFINITIONS */
#define DDGI_WAVE_SIZE 64 // Thread group size for the Trace shader. Sorry AMD, I'm running a 3080
#define DDGI_RAYS_PER_WAVE 3 // This is the number of rays per probe (192) divided by the thread group size (64)
#define DDGI_PROBE_DEPTH_RESOLUTION 16 // Depth is stored as 16x16 FORMAT_R32F texels
#define DDGI_PROBE_IRRADIANCE_RESOLUTION  8 // Irradiance is stored as 6x6 FORMAT_R11G11B10F texels with a 1 pixel border
#define DDGI_RAYS_PER_PROBE DDGI_WAVE_SIZE * DDGI_RAYS_PER_WAVE // Trace 192 rays in total 

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
    float4    mSunDirection;
    float4    mCameraPosition;
    float4x4  mViewMatrix;
    float4x4  mProjectionMatrix;
    float4x4  mViewProjectionMatrix;
    float4x4  mInvViewProjectionMatrix;
    float4x4  mPrevViewProjectionMatrix;
};


struct GbufferRootConstants {
    float4	 mAlbedo;
    uint4	 mTextures;
    float4	 mProperties;
    float4x4 mWorldTransform;
}; 
STATIC_ASSERT(sizeof(GbufferRootConstants) < MAX_ROOT_CONSTANTS_SIZE);


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
    uint mShadowMaskTexture;
    uint mReflectionsTexture;
    uint mGbufferDepthTexture;
    uint mGbufferRenderTexture;
    uint mAmbientOcclusionTexture;
    uint mProbesDepthTexture;
    uint mProbesIrradianceTexture;
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
    uint   mInstancesBuffer;
    uint   mMaterialsBuffer;
    uint   mRaysDepthTexture;
    uint   mRaysIrradianceTexture;
    uint   mTLAS;
    uint   pad0;
    uint   pad1;
    uint   pad2;
    uint4  mDispatchSize;
    uint4  mProbeCount;
    float4 mBBmin;
    float4 mBBmax;
};
STATIC_ASSERT(sizeof(ProbeTraceRootConstants) < MAX_ROOT_CONSTANTS_SIZE);

struct ProbeUpdateRootConstants {
    uint   mRaysDepthTexture;
    uint   mRaysIrradianceTexture;
    uint   mProbesDepthTexture;
    uint   mProbesIrradianceTexture;
    uint3  mDispatchSize;
};
STATIC_ASSERT(sizeof(ProbeUpdateRootConstants) < MAX_ROOT_CONSTANTS_SIZE);


struct ProbeDebugRootConstants {
    uint   mProbesDepthTexture;
    uint   mProbesIrradianceTexture;
    uint   pad0;
    uint   pad1;
    float4 mBBmin;
    float4 mBBmax;
    uint4  mProbeCount;
};
STATIC_ASSERT(sizeof(ProbeDebugRootConstants) < MAX_ROOT_CONSTANTS_SIZE);


#endif // SHARED_H

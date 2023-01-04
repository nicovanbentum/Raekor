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
#endif
    
struct FrameConstants {
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
    float4x4 mViewProj;
};

struct ShadowMaskRootConstants {
    float4x4 invViewProj;
    float4	 mLightDir;
    uint4	 mTextures;
    uint2    mDispatchSize;
    uint	 mFrameCounter;
};

struct LightingRootConstants {
    uint mShadowMaskTexture;
    uint mGbufferDepthTexture;
    uint mGbufferRenderTexture;
};

struct GrassRenderConstants {
    float  mBend;
    float  mTilt;
    float2 mWindDirection;
};

#endif // SHARED_H

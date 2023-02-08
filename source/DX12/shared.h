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

#define STATIC_ASSERT(cond) static_assert(cond);

#else

#define STATIC_ASSERT(cond) 

#endif

#define DDGI_WAVE_SIZE 64 // sorry AMD, I'm running a 3080
#define DDGI_RAYS_PER_WAVE 3
#define MAX_ROOT_CONSTANTS_SIZE 256 // 64 DWORD's as per the DX12 spec


struct Instance {
    uint mIndexBuffer;
    uint mVertexBuffer;
    uint mMaterialIndex;
    float4x4 mLocalToWorldTransform;
};


struct Material {
    float mMetallic;
    float mRoughness;
    uint mAlbedoTexture;
    uint mNormalsTexture;
    uint mMetalRoughTexture;
    float4 mAlbedo;
    float4 mEmissive;
};


struct Vertex {
    float3 mPos;
    float2 mTexCoord;
    float3 mNormal;
    float3 mTangent;
};
  

struct FrameConstants {
    float     mTime;
    float     mDeltaTime;
    float2    mPad0;
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
    uint mGbufferRenderTexture;
    uint mGbufferDepthTexture;
    uint mShadowMaskTexture;
    uint mTLAS;
    uint2 mDispatchSize;
}; 
STATIC_ASSERT(sizeof(ShadowMaskRootConstants) < MAX_ROOT_CONSTANTS_SIZE);


struct ReflectionsRootConstants {
    uint mGbufferRenderTexture;
    uint mGbufferDepthTexture;
    uint mShadowMaskTexture;
    uint mTLAS;
    uint mInstancesBuffer;
    uint mMaterialsBuffer;
    uint2 mDispatchSize;
}; 
STATIC_ASSERT(sizeof(ReflectionsRootConstants) < MAX_ROOT_CONSTANTS_SIZE);


struct LightingRootConstants {
    uint mShadowMaskTexture;
    uint mGbufferDepthTexture;
    uint mGbufferRenderTexture;
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
    uint mTLAS;
    uint3 mDispatchSize;
    float4 mProbeWorldPos;
};
STATIC_ASSERT(sizeof(ProbeTraceRootConstants) < MAX_ROOT_CONSTANTS_SIZE);


#endif // SHARED_H

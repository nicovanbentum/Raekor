#include "include/common.hlsli"
#include "include/random.hlsli"
#include "include/packing.hlsli"

struct RootConstants {
    float4x4 invViewProj;
    float4 mLightDir;
    uint4 mTextures;
    uint2 mDispatchSize;
    uint mFrameCounter;
};

ROOT_CONSTANTS(RootConstants, rc)

float3 ReconstructPosition(float2 uv, float depth, float4x4 InvVP) {
    float x = uv.x * 2.0 - 1.0;
    float y = (1.0 - uv.y) * 2.0 - 1.0;
    float z = depth;
    float4 position_s = float4(x, y, z, 1.0);
    float4 position_v = mul(InvVP, position_s);
    float3 div = position_v.xyz / position_v.w;
    return div;
}

float InterleavedGradientNoise(float2 pixel) {
  float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
  return frac(magic.z * frac(dot(pixel, magic.xy)));
}

uint tea(uint val0, uint val1) {
  uint v0 = val0;
  uint v1 = val1;
  uint s0 = 0;

  for(uint n = 0; n < 16; n++) {
    s0 += 0x9e3779b9;
    v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
    v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
  }

  return v0;
}


[numthreads(8,8,1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    Texture2D<float4> gbuffer = ResourceDescriptorHeap[rc.mTextures.x];
    Texture2D<float> gbuffer_depth = ResourceDescriptorHeap[rc.mTextures.y];
    RWTexture2D<float4> result_texture = ResourceDescriptorHeap[rc.mTextures.z];
    RaytracingAccelerationStructure TLAS = ResourceDescriptorHeap[rc.mTextures.w];


    const float2 pixel_center = float2(threadID.xy) + float2(0.5, 0.5);
    float2 screen_uv = pixel_center / rc.mDispatchSize;

    uint rng = tea(((threadID.y << 16) | threadID.x), rc.mFrameCounter + 1);


    uint ray_flags =  RAY_FLAG_FORCE_OPAQUE |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
    
    RayQuery< RAY_FLAG_FORCE_OPAQUE |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;

    float depth = gbuffer_depth.Load(threadID);
    // if (depth >= 1.0) {
    //     result_texture[threadID.xy] = float4(0.0.xxxx);
    //     return;
    // }

    float2 offset = uniformSampleDisk(pcg_float2(rng), 0.02);

    uint4 gbuffer_packed = asuint(gbuffer[threadID.xy]);
    float3 normal = UnpackNormal(gbuffer_packed.y);
    float3 position = ReconstructPosition(screen_uv, depth, rc.invViewProj);


    RayDesc ray;
    ray.TMin = 0.001;
    ray.TMax = 10000.0;
    ray.Origin = position + normal * 0.01; // TODO: find a more robust method? offsetRay from ray tracing gems 
                                            // expects a geometric normal, which most deferred renderers dont write out
    ray.Direction = -normalize(rc.mLightDir.xyz);

    if (dot(normal, ray.Direction) <= 0) {
        result_texture[threadID.xy] = 0.0.xxxx;
        return;
    }

     query.TraceRayInline(TLAS, ray_flags, 0xFF, ray);
     query.Proceed();

     bool missed = query.CommittedStatus() != COMMITTED_TRIANGLE_HIT;
    
    result_texture[threadID.xy] = float4(missed.xxx, 1.0);
}
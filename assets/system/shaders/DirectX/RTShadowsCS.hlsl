#include "include/bindless.hlsli"
#include "include/packing.hlsli"
#include "include/common.hlsli"
#include "include/random.hlsli"

ROOT_CONSTANTS(ShadowMaskRootConstants, rc)

[numthreads(8,8,1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    Texture2D<float4> gbuffer_texture = ResourceDescriptorHeap[rc.mGbufferRenderTexture];
    Texture2D<float> gbuffer_depth_texture = ResourceDescriptorHeap[rc.mGbufferDepthTexture];
    RWTexture2D<float4> result_texture = ResourceDescriptorHeap[rc.mShadowMaskTexture];
    RaytracingAccelerationStructure TLAS = ResourceDescriptorHeap[rc.mTLAS];
    
    FrameConstants fc = gGetFrameConstants();

    const float2 pixel_center = float2(threadID.xy) + float2(0.5, 0.5);
    float2 screen_uv = pixel_center / rc.mDispatchSize;

    uint rng = TeaHash(((threadID.y << 16) | threadID.x), 1); // TODO: turn 0 into frame counter


    float depth = gbuffer_depth_texture[threadID.xy];
    //if (depth >= 1.0) {
    //    result_texture[threadID.xy] = float4(0.0, 1.0, 0.0, 1.0);
    //    return;
    //}

    const float2 offset = uniformSampleDisk(pcg_float2(rng), 0.02);
    const float3 normal = UnpackNormal(asuint(gbuffer_texture[threadID.xy]).y);
    const float3 position = ReconstructWorldPosition(screen_uv, depth, fc.mInvViewProjectionMatrix);
    
    RayDesc ray;
    ray.TMin = 0.1;
    ray.TMax = 1000.0;
    ray.Origin = position + normal * 0.01; // TODO: find a more robust method? offsetRay from ray tracing gems 
                                            // expects a geometric normal, which most deferred renderers dont write out
    ray.Direction = -normalize(fc.mSunDirection.xyz);

    if (dot(normal, ray.Direction) <= 0) {
        result_texture[threadID.xy] = 0.0.xxxx;
        return;
    }

    uint ray_flags =  RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
    
    RayQuery< RAY_FLAG_FORCE_OPAQUE |
                RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | 
                RAY_FLAG_SKIP_CLOSEST_HIT_SHADER > query;

    query.TraceRayInline(TLAS, ray_flags, 0xFF, ray);
    query.Proceed();

    bool missed = query.CommittedStatus() != COMMITTED_TRIANGLE_HIT;
    result_texture[threadID.xy] = float4(missed.xxx, 1.0);
}
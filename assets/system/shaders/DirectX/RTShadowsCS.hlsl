#include "include/bindless.hlsli"
#include "include/packing.hlsli"
#include "include/common.hlsli"
#include "include/random.hlsli"

ROOT_CONSTANTS(ShadowMaskRootConstants, rc)

[numthreads(8,8,1)]
void main(uint3 threadID : SV_DispatchThreadID) 
{
    if (any(threadID.xy >= rc.mDispatchSize.xy))
        return;

    Texture2D<float4> gbuffer_texture = ResourceDescriptorHeap[rc.mGbufferRenderTexture];
    Texture2D<float> gbuffer_depth_texture = ResourceDescriptorHeap[rc.mGbufferDepthTexture];
    RWTexture2D<float> result_texture = ResourceDescriptorHeap[rc.mShadowMaskTexture];
    RaytracingAccelerationStructure TLAS = ResourceDescriptorHeap[rc.mTLAS];
    
    FrameConstants fc = gGetFrameConstants();

    const float2 pixel_center = float2(threadID.xy) + float2(0.5, 0.5);
    float2 screen_uv = pixel_center / rc.mDispatchSize;

    uint rng = TeaHash(((threadID.y << 16) | threadID.x), fc.mFrameCounter);

    float depth = gbuffer_depth_texture[threadID.xy];
    
    if (depth >= 1.0) {
        result_texture[threadID.xy] = 1.0;
        return;
    }

    const float4 blue_noise = SampleBlueNoise(threadID.xy, fc.mFrameCounter);
    const float2 offset = uniformSampleDisk(blue_noise.xy, fc.mSunConeAngle);
    const float3 normal = UnpackNormal(asuint(gbuffer_texture[threadID.xy]));
    const float3 position = ReconstructWorldPosition(screen_uv, depth, fc.mInvViewProjectionMatrix);
    
    float3 ray_dir = -normalize(fc.mSunDirection.xyz);
    ray_dir.xz += offset;
    
    RayDesc ray;
    ray.TMin = 0.01;
    ray.TMax = 1000.0;
    ray.Origin = position + normal * 0.01; // TODO: find a more robust method? offsetRay from ray tracing gems 
                                            // expects a geometric normal, which most deferred renderers dont write out
    ray.Direction = ray_dir;

    if (dot(normal, ray.Direction) <= 0.0) {
        result_texture[threadID.xy] = 0.0;
        return;
    }

    uint ray_flags =  RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
    
    RayQuery< RAY_FLAG_FORCE_OPAQUE |
                RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | 
                RAY_FLAG_SKIP_CLOSEST_HIT_SHADER > query;

    query.TraceRayInline(TLAS, ray_flags, 0xFF, ray);
    query.Proceed();

    result_texture[threadID.xy] = float(query.CommittedStatus() != COMMITTED_TRIANGLE_HIT);
}
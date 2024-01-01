#include "include/bindless.hlsli"
#include "include/packing.hlsli"
#include "include/common.hlsli"
#include "include/random.hlsli"
#include "include/brdf.hlsli"

ROOT_CONSTANTS(ShadowMaskRootConstants, rc)

groupshared uint g_RayHitsMask;

[numthreads(8, 4, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID, uint groupLocalIndex : SV_GroupIndex)
{
    if (groupLocalIndex == 0)
        g_RayHitsMask = 0;
    
    GroupMemoryBarrierWithGroupSync();
    
    if (any(dispatchThreadID.xy >= rc.mDispatchSize))
        return;
    
    Texture2D<float4> gbuffer_texture = ResourceDescriptorHeap[rc.mGbufferRenderTexture];
    Texture2D<float> gbuffer_depth_texture = ResourceDescriptorHeap[rc.mGbufferDepthTexture];
    RWTexture2D<uint> result_texture = ResourceDescriptorHeap[rc.mShadowMaskTexture];
    RaytracingAccelerationStructure TLAS = ResourceDescriptorHeap[rc.mTLAS];
    
    FrameConstants fc = gGetFrameConstants();

    const float2 pixel_center = float2(dispatchThreadID.xy) + float2(0.5, 0.5);
    float2 screen_uv = pixel_center / rc.mDispatchSize;

    float depth = gbuffer_depth_texture[dispatchThreadID.xy];
    
    uint hit = 0;
    
    uint rng = TeaHash(((dispatchThreadID.y << 16) | dispatchThreadID.x), fc.mFrameCounter + 1);
    
    if (depth < 1.0)
    {
        const float4 blue_noise = SampleBlueNoise(dispatchThreadID.xy, fc.mFrameCounter);
        const float3 ray_dir = SampleDirectionalLight(fc.mSunDirection.xyz, fc.mSunConeAngle, pcg_float2(rng));
        
        const float3 normal     = UnpackNormal(asuint(gbuffer_texture[dispatchThreadID.xy]));
        const float3 position   = ReconstructWorldPosition(screen_uv, depth, fc.mInvViewProjectionMatrix);
    
        RayDesc ray;
        ray.TMin = 0.01;
        ray.TMax = 1000.0;
        ray.Origin = position + normal * 0.01; // TODO: find a more robust method? offsetRay from ray tracing gems 
                                                // expects a geometric normal, which most deferred renderers dont write out
        ray.Direction = ray_dir;

        if (dot(normal, ray.Direction) > 0.0)
        {
            uint ray_flags = RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
            RayQuery < RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER > query;

            query.TraceRayInline(TLAS, ray_flags, 0xFF, ray);
            query.Proceed();
            
            hit = query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;

        }
    }
    
    InterlockedOr(g_RayHitsMask, hit << groupLocalIndex);
    
    GroupMemoryBarrierWithGroupSync();

    if (groupLocalIndex == 0)
        result_texture[groupID.xy] = g_RayHitsMask;
}
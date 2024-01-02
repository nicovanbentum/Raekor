#include "include/bindless.hlsli"
#include "include/packing.hlsli"
#include "include/common.hlsli"
#include "include/random.hlsli"
#include "include/shared.h"

ROOT_CONSTANTS(AmbientOcclusionRootConstants, rc)

[numthreads(8,8,1)]
void main(uint3 threadID : SV_DispatchThreadID) 
{
    if (any(threadID.xy >= rc.mDispatchSize.xy))
        return;

    Texture2D<float4> gbuffer_texture = ResourceDescriptorHeap[rc.mGbufferRenderTexture];
    Texture2D<float> gbuffer_depth_texture = ResourceDescriptorHeap[rc.mGbufferDepthTexture];
    RWTexture2D<float> result_texture = ResourceDescriptorHeap[rc.mAOmaskTexture];
    RaytracingAccelerationStructure TLAS = ResourceDescriptorHeap[rc.mTLAS];
    
    FrameConstants fc = gGetFrameConstants();

    const float2 pixel_center = float2(threadID.xy) + float2(0.5, 0.5);
    float2 screen_uv = pixel_center / rc.mDispatchSize;

    float depth = gbuffer_depth_texture[threadID.xy];
    if (depth >= 1.0) {
        result_texture[threadID.xy] = 0.0;
        return;
    }
    
    float occlusion = 1.0;
    
    float4 blue_noise = SampleBlueNoise(threadID.xy, fc.mFrameCounter);
    const float3 random_offset = SampleCosineWeightedHemisphere(blue_noise.xy);
    const float3 normal = UnpackNormal(asuint(gbuffer_texture[threadID.xy]));
    const float3 ws_position = ReconstructWorldPosition(screen_uv, depth, fc.mInvViewProjectionMatrix);
    
    RayDesc ray;
    ray.TMin = rc.mParams.mNormalBias;
    ray.TMax = rc.mParams.mRadius;
    ray.Origin = ws_position + normal * rc.mParams.mNormalBias; // TODO: find a more robust method? offsetRay from ray tracing gems 
                                            // expects a geometric normal, which most deferred renderers dont write out
    ray.Direction = normalize(normal + random_offset);

    uint ray_flags =  RAY_FLAG_FORCE_OPAQUE;
    
    RayQuery< RAY_FLAG_FORCE_OPAQUE> query;

    query.TraceRayInline(TLAS, ray_flags, 0xFF, ray);
    query.Proceed();
        
    if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        occlusion = saturate((ray.TMin + query.CommittedRayT()) / max(ray.TMax, 0.001));
        occlusion = pow(occlusion, rc.mParams.mPower);
    }
    
    // occlusion /= rc.mParams.mSampleCount;
    
    result_texture[threadID.xy] = occlusion;
}
#include "Include/RayTracing.hlsli"
#include "Include/Bindless.hlsli"
#include "Include/Packing.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"
#include "Include/Shared.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(AmbientOcclusionRootConstants, rc)

[numthreads(8,8,1)]
void main(uint3 threadID : SV_DispatchThreadID) 
{
    if (any(threadID.xy >= rc.mDispatchSize.xy))
        return;
    
    Texture2D<float> depth_texture = ResourceDescriptorHeap[rc.mGbufferDepthTexture];
    Texture2D<float4> gbuffer_texture = ResourceDescriptorHeap[rc.mGbufferRenderTexture];
    RWTexture2D<float> ao_mask_texture = ResourceDescriptorHeap[rc.mAOmaskTexture];
    RaytracingAccelerationStructure TLAS = ResourceDescriptorHeap[fc.mTLAS];

    float2 pixel_center = float2(threadID.xy) + float2(0.5, 0.5);
    float2 screen_uv = pixel_center / rc.mDispatchSize;

    float occlusion = 1.0;
    float depth = depth_texture[threadID.xy];
    
    if (depth)
    {
        float3 normal = UnpackNormal(asuint(gbuffer_texture[threadID.xy]));
        float3 ws_position = ReconstructWorldPosition(screen_uv, depth, fc.mInvViewProjectionMatrix);
        float3 vs_position = mul(fc.mViewMatrix, float4(ws_position, 1.0)).xyz;

        float4 blue_noise = SampleBlueNoise(threadID.xy, fc.mFrameCounter);
        float3 hemi = SampleCosineWeightedHemisphere(blue_noise.xy);
        float3x3 tbn = BuildOrthonormalBasis(normal);
        float3 new_normal = mul(tbn, hemi);
    
        float bias = (-vs_position.z + length(ws_position.xyz)) * 1e-3;
    
        RayDesc ray;
        ray.TMin = 0.0;
        ray.TMax = rc.mParams.mRadius;
        ray.Origin = ws_position + normal * bias;
        ray.Direction = normalize(new_normal);
    
        uint ray_flags = RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
        RayQuery < RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER > query;

        query.TraceRayInline(TLAS, ray_flags, 0xFF, ray);
        query.Proceed();

        
        if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        {
            occlusion = saturate((ray.TMin + query.CommittedRayT()) / max(ray.TMax, 0.001));
            occlusion = pow(occlusion, rc.mParams.mPower);
        }
        // occlusion /= rc.mParams.mSampleCount;
    }
        
    
    ao_mask_texture[threadID.xy] = occlusion;
}
#include "Include/Bindless.hlsli"
#include "Include/Material.hlsli"
#include "Include/Packing.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(ShadowMaskRootConstants, rc)

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (any(dispatchThreadID.xy >= rc.mDispatchSize))
        return;
    
    Texture2D<float4> gbuffer_texture = ResourceDescriptorHeap[rc.mGbufferRenderTexture];
    Texture2D<float> gbuffer_depth_texture = ResourceDescriptorHeap[rc.mGbufferDepthTexture];
    RWTexture2D<float> result_texture = ResourceDescriptorHeap[rc.mShadowMaskTexture];
    RaytracingAccelerationStructure TLAS = ResourceDescriptorHeap[fc.mShadowTLAS];

    const float2 pixel_center = float2(dispatchThreadID.xy) + float2(0.5f, 0.5f);
    float2 screen_uv = pixel_center / rc.mDispatchSize;

    float depth = gbuffer_depth_texture[dispatchThreadID.xy];
    
    uint hit = 0;
    
    if (depth < 1.0f)
    {
        uint rng = TeaHash(((dispatchThreadID.y << 16) | dispatchThreadID.x), fc.mFrameCounter + 1);
        float4 blue_noise = SampleBlueNoise(dispatchThreadID.xy, fc.mFrameCounter);
        float3 ray_dir = SampleDirectionalLight(fc.mSunDirection.xyz, fc.mSunConeAngle, pcg_float2(rng));
        
        float3 normal = UnpackNormal(asuint(gbuffer_texture[dispatchThreadID.xy]));
        float3 ws_pos = ReconstructWorldPosition(screen_uv, depth, fc.mInvViewProjectionMatrix);
        float3 vs_pos = mul(fc.mViewMatrix, float4(ws_pos, 1.0)).xyz;

        float bias = (-vs_pos.z + length(ws_pos.xyz)) * 1e-3;

        RayDesc ray;
        ray.TMin = 0.0f;
        ray.TMax = 10000.0f;
        ray.Origin = ws_pos + normal * 0.01f;
        ray.Direction = ray_dir;

        if (dot(normal, ray.Direction) > 0.0)
        {
            uint ray_flags = RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
            RayQuery < RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER > query;

            query.TraceRayInline(TLAS, ray_flags, 0xFF, ray);
            query.Proceed();
            
            hit = query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
        }
        else
        {
            hit = true;
        }
    }
    
    result_texture[dispatchThreadID.xy] = hit ? 0.0f : 1.0f;
}
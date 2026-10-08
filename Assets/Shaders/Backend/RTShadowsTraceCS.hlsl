#include "Include/Bindless.hlsli"
#include "Include/Material.hlsli"
#include "Include/Packing.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(ShadowMaskRootConstants, rc)

#define SHADOW_RAY_MIN_BIAS 0.01f
#define SHADOW_RAY_DISTANCE_BIAS 2e-4f


float3 ReconstructNeighbourPosition(Texture2D<float> inDepthTexture, int2 inPixel, out float outDepth)
{
    const int2 pixel = clamp(inPixel, int2(0, 0), int2(rc.mDispatchSize) - 1);
    outDepth = inDepthTexture[pixel];
    return ReconstructWorldPosition((float2(pixel) + 0.5f) / float2(rc.mDispatchSize), outDepth, fc.mInvViewProjectionMatrix);
}


float3 ReconstructGeometricNormal(Texture2D<float> inDepthTexture, int2 inPixel, float inDepth, float3 inPosition, float3 inFallbackNormal)
{
    float left_depth, right_depth, up_depth, down_depth;
    const float3 left = ReconstructNeighbourPosition(inDepthTexture, inPixel - int2(1, 0), left_depth);
    const float3 right = ReconstructNeighbourPosition(inDepthTexture, inPixel + int2(1, 0), right_depth);
    const float3 up = ReconstructNeighbourPosition(inDepthTexture, inPixel - int2(0, 1), up_depth);
    const float3 down = ReconstructNeighbourPosition(inDepthTexture, inPixel + int2(0, 1), down_depth);

    const bool use_left = abs(left_depth - inDepth) < abs(right_depth - inDepth);
    const bool use_up = abs(up_depth - inDepth) < abs(down_depth - inDepth);

    if (( use_left ? left_depth : right_depth ) >= 1.0f || ( use_up ? up_depth : down_depth ) >= 1.0f)
        return inFallbackNormal;

    const float3 tangent_x = use_left ? inPosition - left : right - inPosition;
    const float3 tangent_y = use_up ? inPosition - up : down - inPosition;

    float3 normal = cross(tangent_y, tangent_x);

    if (dot(normal, normal) < 1e-12f)
        return inFallbackNormal;

    normal = normalize(normal);

    return dot(normal, fc.mCameraPosition.xyz - inPosition) < 0.0f ? -normal : normal;
}


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
        
        float3 ws_pos = ReconstructWorldPosition(screen_uv, depth, fc.mInvViewProjectionMatrix);
        float3 shading_normal = UnpackNormal(asuint(gbuffer_texture[dispatchThreadID.xy]));
        float3 normal = ReconstructGeometricNormal(gbuffer_depth_texture, int2(dispatchThreadID.xy), depth, ws_pos, shading_normal);

        const float view_distance = length(fc.mCameraPosition.xyz - ws_pos);

        RayDesc ray;
        ray.TMin = 0.0f;
        ray.TMax = 10000.0f;
        ray.Origin = ws_pos + normal * (SHADOW_RAY_MIN_BIAS + view_distance * SHADOW_RAY_DISTANCE_BIAS);
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
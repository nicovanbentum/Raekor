#include "include/bindless.hlsli"
#include "include/packing.hlsli"
#include "include/common.hlsli"
#include "include/random.hlsli"
#include "include/brdf.hlsli"
#include "include/sky.hlsli"
#include "include/rt.hlsli"

ROOT_CONSTANTS(ReflectionsRootConstants, rc)

[numthreads(8,8,1)]
void main(uint3 threadID : SV_DispatchThreadID) 
{
    if (any(threadID.xy >= rc.mDispatchSize.xy))
        return;

    Texture2D<float4> gbuffer_texture = ResourceDescriptorHeap[rc.mGbufferRenderTexture];
    Texture2D<float> gbuffer_depth_texture = ResourceDescriptorHeap[rc.mGbufferDepthTexture];
    RWTexture2D<float4> result_texture = ResourceDescriptorHeap[rc.mShadowMaskTexture];
    
    RaytracingAccelerationStructure TLAS = ResourceDescriptorHeap[rc.mTLAS];
    StructuredBuffer<RTGeometry> geometries = ResourceDescriptorHeap[rc.mInstancesBuffer];
    StructuredBuffer<RTMaterial> materials  = ResourceDescriptorHeap[rc.mMaterialsBuffer];
    
    FrameConstants fc = gGetFrameConstants();

    const float2 pixel_center = float2(threadID.xy) + float2(0.5, 0.5);
    float2 screen_uv = pixel_center / rc.mDispatchSize;

    uint rng = TeaHash(((threadID.y << 16) | threadID.x), fc.mFrameCounter + 1);

    float depth = gbuffer_depth_texture[threadID.xy];
    if (depth >= 1.0) {
        result_texture[threadID.xy] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    BRDF gbuffer_brdf;
    gbuffer_brdf.Unpack(asuint(gbuffer_texture[threadID.xy]));

    if (gbuffer_brdf.mRoughness >= 0.3 && gbuffer_brdf.mMetallic < 0.1) {
        result_texture[threadID.xy] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    
    const float3 position = ReconstructWorldPosition(screen_uv, depth, fc.mInvViewProjectionMatrix);
    
    const float3 Wo = normalize(fc.mCameraPosition.xyz - position.xyz);
    
    float3 Wi, Wh;
    gbuffer_brdf.SampleSpecular(rng, Wo, Wi, Wh);
    
    RayDesc ray;
    ray.TMin = 0.1;
    ray.TMax = 1000.0;
    ray.Origin = position + gbuffer_brdf.mNormal * 0.01; // TODO: find a more robust method? offsetRay from ray tracing gems 
                                            // expects a geometric normal, which most deferred renderers dont write out
    ray.Direction = Wi;

    uint ray_flags = RAY_FLAG_FORCE_OPAQUE;
    
    RayQuery < RAY_FLAG_FORCE_OPAQUE > query;

    query.TraceRayInline(TLAS, ray_flags, 0xFF, ray);
    query.Proceed();
    

    if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        // Calculate vertex and material from the hit info
        RTGeometry geometry = geometries[query.CommittedInstanceID()];
        RTVertex vertex = CalculateVertexFromGeometry(geometry, query.CommittedPrimitiveIndex(), query.CommittedTriangleBarycentrics());
        RTMaterial material = materials[geometry.mMaterialIndex];
        
        // transform to world space
        TransformToWorldSpace(vertex, geometry.mLocalToWorldTransform, geometry.mInvLocalToWorldTransform);
        vertex.mNormal = normalize(vertex.mNormal);
        vertex.mTangent = normalize(vertex.mTangent);
        vertex.mTangent = normalize(vertex.mTangent - dot(vertex.mTangent, vertex.mNormal) * vertex.mNormal);
            
        const float3 Wo = normalize(fc.mCameraPosition.xyz - vertex.mPos);
        const float3 Wi = normalize(-fc.mSunDirection.xyz);
        const float3 Wh = normalize(Wo + Wi);
        
        BRDF brdf;
        brdf.FromHit(vertex, material);
        
        const float3 l = brdf.Evaluate(Wo, Wi, Wh);

        const float NdotL = max(dot(brdf.mNormal, Wi), 0.0);
        float3 sunlight_luminance = Absorb(IntegrateOpticalDepth(0.xxx, -Wi));
        float3 total_radiance = l * NdotL * sunlight_luminance;
        
        if (NdotL != 0.0) {
            RayDesc shadow_ray;
            shadow_ray.Origin = vertex.mPos + brdf.mNormal * 0.01;
            shadow_ray.Direction = Wi;
            shadow_ray.TMin = 0.1;
            shadow_ray.TMax = 1000.0;
            
            float4 blue_noise = SampleBlueNoise(threadID.xy, fc.mFrameCounter);
            shadow_ray.Direction.xz += uniformSampleDisk(blue_noise.xy, 0.00);
            
            uint shadow_ray_flags = RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
    
            RayQuery <RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER> shadow_query;

            shadow_query.TraceRayInline(TLAS, shadow_ray_flags, 0xFF, shadow_ray);
            shadow_query.Proceed();
        
            total_radiance *= shadow_query.CommittedStatus() != COMMITTED_TRIANGLE_HIT;
        }
        
        
        result_texture[threadID.xy] = float4(total_radiance, 1.0);
    }
    else
    {
        // Calculate sky
        float3 transmittance;
        float3 inscattering = IntegrateScattering(ray.Origin, -ray.Direction, 1.#INF, fc.mSunDirection.xyz, fc.mSunColor.rgb, transmittance);
            
        result_texture[threadID.xy] = float4(max(inscattering, 0.0.xxx) * fc.mSunColor.a, 1.0);
    }
    
    //result_texture[threadID.xy] = float4(normal, 1.0);
}
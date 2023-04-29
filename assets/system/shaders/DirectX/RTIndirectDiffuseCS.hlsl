#include "include/bindless.hlsli"
#include "include/packing.hlsli"
#include "include/common.hlsli"
#include "include/random.hlsli"
#include "include/brdf.hlsli"
#include "include/sky.hlsli"
#include "include/rt.hlsli"

ROOT_CONSTANTS(ReflectionsRootConstants, rc)

[numthreads(8,8,1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    Texture2D<float4> gbuffer_texture = ResourceDescriptorHeap[rc.mGbufferRenderTexture];
    Texture2D<float> gbuffer_depth_texture = ResourceDescriptorHeap[rc.mGbufferDepthTexture];
    RWTexture2D<float4> result_texture = ResourceDescriptorHeap[rc.mShadowMaskTexture];
    
    RaytracingAccelerationStructure TLAS = ResourceDescriptorHeap[rc.mTLAS];
    StructuredBuffer<RTGeometry> geometries = ResourceDescriptorHeap[rc.mInstancesBuffer];
    StructuredBuffer<RTMaterial> materials  = ResourceDescriptorHeap[rc.mMaterialsBuffer];
    
    FrameConstants fc = gGetFrameConstants();

    const float2 pixel_center = float2(threadID.xy) + float2(0.5, 0.5);
    float2 screen_uv = pixel_center / rc.mDispatchSize;
    
    uint rng = TeaHash(((threadID.y << 16) | threadID.x), fc.mFrameCounter);

    float depth = gbuffer_depth_texture[threadID.xy];
    if (depth >= 1.0) {
        result_texture[threadID.xy] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    const uint4 gbuffer_sample = asuint(gbuffer_texture[threadID.xy]);
    
    const float3 normal = UnpackNormal(gbuffer_sample);
    const float4 gbuffer_albedo = UnpackAlbedo(gbuffer_sample);
    float3 ws_position = ReconstructWorldPosition(screen_uv, depth, fc.mInvViewProjectionMatrix);
    float3 random_offset = SampleCosineWeightedHemisphere(pcg_float2(rng));
    
    RayDesc ray;
    ray.TMin = 0.01;
    ray.TMax = 10000.0;
    ray.Origin = ws_position + normal * 0.01; // TODO: find a more robust method?
    ray.Direction = normalize(normal + random_offset);
    
    static const uint bounces = 2;
    
    float3 total_irradiance = 0.0.xxx;
    float3 throughput = 1.0.xxx;
    
    for (uint bounce = 0; bounce < bounces; bounce++)
    {
        uint ray_flags = RAY_FLAG_FORCE_OPAQUE;
    
        RayQuery < RAY_FLAG_FORCE_OPAQUE > query;

        query.TraceRayInline(TLAS, ray_flags, 0xFF, ray);
        query.Proceed();
    
        // Handle hit case
        if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        {
            // Calculate vertex and material from the hit info
            RTGeometry geometry = geometries[query.CommittedInstanceID()];
            RTVertex vertex = CalculateVertexFromGeometry(geometry, query.CommittedPrimitiveIndex(), query.CommittedTriangleBarycentrics());
            RTMaterial material = materials[geometry.mMaterialIndex];
        
            // Setup the BRDF
            BRDF brdf;
            brdf.FromHit(vertex, material);
        
            // Evalulate the BRDF
            const float3 Wo = -ray.Direction;
            const float3 Wi = normalize(-fc.mSunDirection.xyz);
            const float3 Wh = normalize(Wo + Wi);
            const float3 l = brdf.Evaluate(Wo, Wi, Wh);
        
            // Calculate irradiance
            const float NdotL = max(dot(brdf.mNormal, Wi), 0.0);
            float3 sunlight_luminance = Absorb(IntegrateOpticalDepth(0.xxx, -Wi));
            float3 irradiance = l * NdotL * sunlight_luminance;
        
            // Check if the sun is visible
            if (NdotL != 0.0) {
                RayDesc shadow_ray;
                shadow_ray.Origin = vertex.mPos + brdf.mNormal * 0.01;
                shadow_ray.Direction = Wi;
                shadow_ray.TMin = 0.1;
                shadow_ray.TMax = 1000.0;
        
                uint shadow_ray_flags = RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
    
                RayQuery <RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER> shadow_query;

                shadow_query.TraceRayInline(TLAS, shadow_ray_flags, 0xFF, shadow_ray);
                shadow_query.Proceed();
        
                irradiance *= shadow_query.CommittedStatus() != COMMITTED_TRIANGLE_HIT;
            }
        
            // Update irradiance and throughput
            total_irradiance += irradiance * throughput;
            throughput *= brdf.mAlbedo.rgb;
            
            // Update ray info, new ray starts from the current intersection
            random_offset = SampleCosineWeightedHemisphere(pcg_float2(rng));
            ray.Origin = vertex.mPos + vertex.mNormal * 0.01; // TODO: find a more robust method?
            ray.Direction = normalize(vertex.mNormal + random_offset);
        }
        else // Handle miss case
        {
            // Calculate sky
            float3 transmittance;
            float3 inscattering = IntegrateScattering(ray.Origin, ray.Direction, 1.#INF, fc.mSunDirection.xyz, float3(1, 1, 1), transmittance);
            total_irradiance += inscattering * throughput;
            
            // Stop tracing
            break;
        }
        
        
    }
    result_texture[threadID.xy] = float4(total_irradiance, 1.0);

}
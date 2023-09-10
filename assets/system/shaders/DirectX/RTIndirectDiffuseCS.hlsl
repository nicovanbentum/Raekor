#include "include/bindless.hlsli"
#include "include/packing.hlsli"
#include "include/common.hlsli"
#include "include/random.hlsli"
#include "include/brdf.hlsli"
#include "include/sky.hlsli"
#include "include/rt.hlsli"

ROOT_CONSTANTS(PathTraceRootConstants, rc)



[numthreads(8,8,1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    RWTexture2D<float4> result_texture = ResourceDescriptorHeap[rc.mResultTexture];
    
    RaytracingAccelerationStructure TLAS = ResourceDescriptorHeap[rc.mTLAS];
    StructuredBuffer<RTGeometry> geometries = ResourceDescriptorHeap[rc.mInstancesBuffer];
    StructuredBuffer<RTMaterial> materials  = ResourceDescriptorHeap[rc.mMaterialsBuffer];
    
    const FrameConstants fc = gGetFrameConstants();
    
    uint rng = TeaHash(((threadID.y << 16) | threadID.x), fc.mFrameCounter + 1);

    const float2 pixel_center = float2(threadID.xy) + float2(0.5, 0.5);
    const float2 offset_pixel_center = pixel_center + pcg_float2(rng);
    
    const float2 screen_uv = offset_pixel_center / rc.mDispatchSize;
    
    const float2 clip = float2(screen_uv.x * 2.0 - 1.0, (1.0 - screen_uv.y) * 2.0 - 1.0);
    float4 target = normalize(mul(fc.mInvViewProjectionMatrix, float4(clip.x, clip.y, 0.0, 1.0)));
    target /= target.w;

    RayDesc ray;
    ray.TMin = 0.001;
    ray.TMax = 10000.0;
    ray.Origin = fc.mCameraPosition.xyz;
    ray.Direction = normalize(target.xyz - ray.Origin);
    
    float3 total_irradiance = 0.0.xxx;
    float3 total_throughput = 1.0.xxx;
    
    for (uint bounce = 0; bounce < rc.mBounces; bounce++)
    {
        uint ray_flags = RAY_FLAG_FORCE_OPAQUE;
    
        RayQuery < RAY_FLAG_FORCE_OPAQUE > query;

        query.TraceRayInline(TLAS, ray_flags, 0xFF, ray);
        query.Proceed();
        
        float3 irradiance = 0.0.xxx;
        float3 throughput = 1.0.xxx;
    
        // Handle hit case
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
            
            // Setup the BRDF
            BRDF brdf;
            brdf.FromHit(vertex, material);
            
            // brdf.mAlbedo.rgb *= 4.0f;
            
            irradiance = material.mEmissive.rgb;
            
            const float3 Wo = -ray.Direction;
            
            {
                const float3 light_dir = normalize(fc.mSunDirection.xyz);
                float2 diskPoint = uniformSampleDisk(pcg_float2(rng), 0.00);
                float3 Wi = -(light_dir + float3(diskPoint.x, 0.0, diskPoint.y));
            
                // Check if the sun is visible
                RayDesc shadow_ray;
                shadow_ray.Origin = vertex.mPos + vertex.mNormal * 0.01;
                shadow_ray.Direction = Wi;
                shadow_ray.TMin = 0.1;
                shadow_ray.TMax = 1000.0;
                
                uint shadow_ray_flags = RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
    
                RayQuery <RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER> shadow_query;

                shadow_query.TraceRayInline(TLAS, shadow_ray_flags, 0xFF, shadow_ray);
                shadow_query.Proceed();
                
                float3 sunlight_luminance = Absorb(IntegrateOpticalDepth(0.xxx, -Wi)) * fc.mSunColor.a;
                sunlight_luminance *= shadow_query.CommittedStatus() != COMMITTED_TRIANGLE_HIT;
                
                const float NdotL = max(dot(brdf.mNormal, Wi), 0.0);
                
                const float3 Wh = normalize(Wo + Wi);
                
                // Evalulate the BRDF
                const float3 l = brdf.Evaluate(Wo, Wi, Wh);
                
                // Add irradiance
                irradiance += l * NdotL * sunlight_luminance;
            }
            
            { // sample the BRDF to get new outgoing direction , update ray dir and pos
                brdf.Sample(rng, Wo, ray.Direction, throughput);
                ray.Origin = vertex.mPos + vertex.mNormal * 0.01; // TODO: find a more robust method?
            }
            
            // // Russian roulette
            if (bounce > 3) {
                const float r = pcg_float(rng);
                const float p = max(brdf.mAlbedo.r, max(brdf.mAlbedo.g, brdf.mAlbedo.b));
                if (r > p)
                    break;
                else
                    throughput = (1.0 / p).xxx;
            }
            
            // debug
            //total_irradiance = brdf.mNormal * 0.5 + 0.5;
            //break;
        }
        else // Handle miss case 
        {
            // Calculate sky
            float3 transmittance;
            float3 inscattering = IntegrateScattering(ray.Origin, -ray.Direction, 1.#INF, fc.mSunDirection.xyz, float3(1, 1, 1), transmittance);
            
            irradiance = min(inscattering, 1.0.xxx) * fc.mSunColor.a;
            
            // Stop tracing
            bounce = rc.mBounces + 1;
        }
      
        // Update irradiance and throughput
        total_irradiance += irradiance * total_throughput;
        total_throughput *= throughput;
    }
    
    float4 curr = result_texture[threadID.xy];
    
    result_texture[threadID.xy] = float4(total_irradiance, 1.0);

}
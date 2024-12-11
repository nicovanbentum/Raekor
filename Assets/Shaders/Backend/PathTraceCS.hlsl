#include "Include/Bindless.hlsli"
#include "Include/Packing.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"
#include "Include/Material.hlsli"
#include "Include/Sky.hlsli"
#include "Include/RayTracing.hlsli"

ROOT_CONSTANTS(PathTraceRootConstants, rc)



[numthreads(8,8,1)]
void main(uint3 threadID : SV_DispatchThreadID) 
{
    if (any(threadID.xy >= rc.mDispatchSize.xy))
        return;
    
    RWTexture2D<float4> result_texture       = ResourceDescriptorHeap[rc.mResultTexture];
    RWTexture2D<float4> accumulation_texture = ResourceDescriptorHeap[rc.mAccumulationTexture];
    
    RaytracingAccelerationStructure TLAS    = ResourceDescriptorHeap[rc.mTLAS];
    StructuredBuffer<RTLight> lights        = ResourceDescriptorHeap[rc.mLightsBuffer];
    StructuredBuffer<RTGeometry> geometries = ResourceDescriptorHeap[rc.mInstancesBuffer];
    StructuredBuffer<RTMaterial> materials  = ResourceDescriptorHeap[rc.mMaterialsBuffer];

    TextureCube<float3> skycube_texture    = ResourceDescriptorHeap[rc.mSkyCubeTexture];
    
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
        
        float opacity = 1.0;
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
            TransformToWorldSpace(vertex, geometry.mWorldTransform);
            vertex.mNormal = normalize(vertex.mNormal);
            vertex.mTangent = normalize(vertex.mTangent);
            vertex.mTangent = normalize(vertex.mTangent - dot(vertex.mTangent, vertex.mNormal) * vertex.mNormal);
            
            // Setup the BRDF
            BRDF brdf;
            brdf.FromHit(vertex, material);
            
            brdf.mAlbedo.rgb *= opacity;
            opacity = opacity * (1 - brdf.mAlbedo.a);
            
            // brdf.mAlbedo.rgb *= 4.0f;
            //wtf
            
            irradiance = brdf.mEmissive;
            
            const float3 Wo = -ray.Direction;

            {
                // sample a ray direction towards the sun disk
                float3 Wi = SampleDirectionalLight(fc.mSunDirection.xyz, fc.mSunConeAngle, pcg_float2(rng));
            
                // Check if the sun is visible
                bool hit = TraceShadowRay(TLAS, vertex.mPos + vertex.mNormal * 0.01, Wi, 0.1f, 1000.0f);
                
                if (!hit)
                    irradiance += EvaluateDirectionalLight(brdf, fc.mSunColor, Wi, Wo);
            }
            
            // for (uint i = 0; i < 8; i++)
            {
                uint random_light_index = uint(round(float(rc.mLightsCount - 1) * pcg_float(rng)));
                RTLight light = lights[random_light_index];
                
                switch (light.mType)
                {
                    case RT_LIGHT_TYPE_POINT:
                    {
                            float3 Wi = SamplePointLight(light, vertex.mPos);
                            float t_max = length(light.mPosition.xyz - vertex.mPos);
                            
                            float point_radius = light.mAttributes.x * sqrt(pcg_float(rng));
                            float point_angle = pcg_float(rng) * 2.0f * M_PI;
                            float2 disk_point = float2(point_radius * cos(point_angle), point_radius * sin(point_angle));
                                
                            bool hit = TraceShadowRay(TLAS, vertex.mPos + vertex.mNormal * 0.01, Wi, 2.0f, t_max);
                        
                            if (!hit)
                                irradiance += EvaluatePointLight(brdf, light, Wi, Wo, t_max);
                        }
                        break;
                    
                    case RT_LIGHT_TYPE_SPOT:
                    {
                            float3 Wi = SampleSpotLight(light, vertex.mPos);
                            float t_max = length(light.mPosition.xyz - vertex.mPos);

                            float point_radius = 0.022f;
                            float point_angle = pcg_float(rng) * 2.0f * M_PI;
                            float2 disk_point = float2(point_radius * cos(point_angle), point_radius * sin(point_angle));
                                        
                            bool hit = TraceShadowRay(TLAS, vertex.mPos + vertex.mNormal * 0.01, float3(Wi.x + disk_point.x, Wi.y, Wi.z + disk_point.y), 2.0f, t_max);
                        
                            if (!hit)
                                irradiance += EvaluateSpotLight(brdf, light, Wi, Wo, t_max);
                        }
                        break;
                }
            }

            { // sample the BRDF to get new outgoing direction , update ray dir and pos
                float3 Wi;
                brdf.Sample(rng, Wo, Wi, throughput);
                
                ray.Origin = vertex.mPos + vertex.mNormal * 0.01; // TODO: find a more robust method?
                ray.Direction = Wi;
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
            irradiance = skycube_texture.SampleLevel(SamplerLinearClamp, -ray.Direction, 0);
            irradiance = max(irradiance, 0.0.xxx) * fc.mSunColor.a;
            
            // Stop tracing
            bounce = rc.mBounces + 1;
        }
      
        // Update irradiance and throughput
        total_irradiance += irradiance * total_throughput;
        total_throughput *= throughput;
    }
    
    if (rc.mReset || fc.mFrameCounter == 0)
    {
        result_texture[threadID.xy] = float4(total_irradiance, 1.0);
        accumulation_texture[threadID.xy] = float4(total_irradiance, 1.0);
    }
    else
    {
        float4 acc = accumulation_texture[threadID.xy];
        acc += float4(total_irradiance, 1.0);
        
        result_texture[threadID.xy] = float4(acc.rgb / acc.a, 1.0);
        accumulation_texture[threadID.xy] = acc;
    }
}
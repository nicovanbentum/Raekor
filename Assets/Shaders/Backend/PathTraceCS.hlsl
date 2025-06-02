#include "Include/Bindless.hlsli"
#include "Include/Packing.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"
#include "Include/Material.hlsli"
#include "Include/Sky.hlsli"
#include "Include/RayTracing.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(PathTraceRootConstants, rc)

[numthreads(8,8,1)]
void main(uint3 threadID : SV_DispatchThreadID) 
{
    if (any(threadID.xy >= rc.mDispatchSize.xy))
        return;

    RWTexture2D<float> depth_texture         = ResourceDescriptorHeap[rc.mDepthTexture];
    RWTexture2D<float4> result_texture       = ResourceDescriptorHeap[rc.mResultTexture];
    TextureCube<float3> skycube_texture      = ResourceDescriptorHeap[rc.mSkyCubeTexture];
    RWTexture2D<uint> selection_texture      = ResourceDescriptorHeap[rc.mSelectionTexture];
    RWTexture2D<float4> accumulation_texture = ResourceDescriptorHeap[rc.mAccumulationTexture];
    
    RaytracingAccelerationStructure TLAS        = ResourceDescriptorHeap[fc.mTLAS];
    RaytracingAccelerationStructure shadowTLAS  = ResourceDescriptorHeap[fc.mShadowTLAS];
    StructuredBuffer<RTLight> lights            = ResourceDescriptorHeap[fc.mLightsBuffer];
    StructuredBuffer<RTGeometry> geometries     = ResourceDescriptorHeap[fc.mInstancesBuffer];
    StructuredBuffer<RTMaterial> materials      = ResourceDescriptorHeap[fc.mMaterialsBuffer];

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
    
    uint entity = 0;
    float depth = 1.0f;
    
    float3 total_irradiance = 0.0.xxx;
    float3 total_throughput = 1.0.xxx;
    
    float opacity = 1.0;
    int alpha_bounces = 0;
    
    for (int bounce = 0; bounce < rc.mBounces; bounce++)
    {
        uint ray_flags = RAY_FLAG_FORCE_OPAQUE;
    
        RayQuery < RAY_FLAG_FORCE_OPAQUE > query;

        query.TraceRayInline(TLAS, ray_flags, 0xFF, ray);
        while (query.Proceed()) {}
        
        float3 irradiance = 0.0.xxx;
        float3 throughput = 1.0.xxx;
    
        // Handle hit case
        if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        {
            // Calculate vertex and material from the hit info
            RTGeometry geometry = geometries[query.CommittedInstanceID()];
            RTVertex vertex = CalculateVertexFromGeometry(geometry, query.CommittedPrimitiveIndex(), query.CommittedTriangleBarycentrics());
            RTMaterial material = materials[geometry.mMaterialIndex];

            // Transform to world space
            TransformToWorldSpace(vertex, geometry.mWorldTransform);
            
            // Setup the surface
            Surface surface;
            surface.FromHit(vertex, material);
            
            // Store GBuffer values on first hit
            if (bounce == 0)
            {
                float4 pos = mul(fc.mViewProjectionMatrix, float4(vertex.mPos, 1.0));
                depth = (pos.xyz / pos.w).z;
                entity = geometry.mEntity;
            }
            
            // Handle transparency
            if (surface.mAlbedo.a < 0.5)
            {
                ray.Origin = vertex.mPos + ray.Direction * 0.001;
                continue;
            }
            
            // Handle backfaces
            if (!query.CommittedTriangleFrontFace())
            {
                surface.mNormal = -surface.mNormal;
            }

            // Handle emissive
            irradiance = surface.mEmissive;
            const float3 Wo = -ray.Direction;

            // Next event estimation
            // Handle sunlight
            {
                float3 Wi = SampleDirectionalLight(fc.mSunDirection.xyz, fc.mSunConeAngle, pcg_float2(rng));
            
                bool hit = TraceShadowRay(shadowTLAS, vertex.mPos + vertex.mNormal * 0.01, Wi, 0.1f, 1000.0f);
                
                if (!hit)
                    irradiance += EvaluateDirectionalLight(surface, fc.mSunColor, Wi, Wo);
            }

            // Handle point and spot lights 
            // Randomly select 1 every frame
            uint random_light_index = uint(round(float(fc.mNrOfLights - 1) * pcg_float(rng)));
            RTLight light = lights[random_light_index];
                
            switch (light.mType)
            {
                case RT_LIGHT_TYPE_POINT:
                {
                        float3 Wi = SamplePointLight(light, vertex.mPos);
                        
                        float t_min = light.mAttributes.y;
                        float t_max = length(light.mPosition.xyz - vertex.mPos);
                            
                        float point_radius = light.mAttributes.x * sqrt(pcg_float(rng));
                        float point_angle = pcg_float(rng) * 2.0f * M_PI;
                        float2 disk_point = float2(point_radius * cos(point_angle), point_radius * sin(point_angle));
                                
                        bool hit = TraceShadowRay(TLAS, vertex.mPos + vertex.mNormal * 0.01, Wi, t_min, t_max);
                        
                        if (!hit)
                            irradiance += EvaluatePointLight(surface, light, Wi, Wo, t_max);
                    }
                    break;
                    
                case RT_LIGHT_TYPE_SPOT:
                {
                        float3 Wi = SampleSpotLight(light, vertex.mPos);
                        float t_max = length(light.mPosition.xyz - vertex.mPos);

                        float point_radius = 0.022f;
                        float point_angle = pcg_float(rng) * 2.0f * M_PI;
                        float2 disk_point = float2(point_radius * cos(point_angle), point_radius * sin(point_angle));
                        
                        float3 light_dir = float3(Wi.x + disk_point.x, Wi.y, Wi.z + disk_point.y);
                                        
                        bool hit = TraceShadowRay(TLAS, vertex.mPos + vertex.mNormal * 0.01, light_dir, 2.0f, t_max);
                        
                        if (!hit)
                            irradiance += EvaluateSpotLight(surface, light, Wi, Wo, t_max);
                    }
                    break;
            }

            // Sample the BRDF to get new outgoing direction, update ray dir and pos
            { 
                ray.Origin = vertex.mPos + vertex.mNormal * 0.01;
                surface.SampleBRDF(rng, Wo, ray.Direction, throughput);
            }
            
            // Russian roulette
            if (bounce > 3) 
            {
                const float r = pcg_float(rng);
                const float p = max(surface.mAlbedo.r, max(surface.mAlbedo.g, surface.mAlbedo.b));
                
                if (r > p)
                    break;
                else
                    throughput = (1.0 / p).xxx;
            }
        }
        else // Handle miss case 
        {
            // Calculate sky
            irradiance = skycube_texture.SampleLevel(SamplerLinearClamp, ray.Direction, 0);
            irradiance = max(irradiance, 0.0.xxx) * fc.mSunColor.a;
            
            // Stop tracing
            bounce = rc.mBounces + 1;
        }
      
        // Prevent fireflies
        irradiance = min(irradiance, 10.0.xxx);

        // Update irradiance and throughput
        total_irradiance += irradiance * total_throughput;
        total_throughput *= throughput;
    }
    
    // Output to textures
    depth_texture[threadID.xy] = depth;
    selection_texture[threadID.xy] = entity;
    
    if (rc.mReset || fc.mFrameCounter < 2)
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
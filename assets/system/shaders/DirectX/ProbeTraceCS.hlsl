#include "include/bindless.hlsli"
#include "include/packing.hlsli"
#include "include/common.hlsli"
#include "include/random.hlsli"
#include "include/ddgi.hlsli"
#include "include/brdf.hlsli"
#include "include/sky.hlsli"
#include "include/rt.hlsli"

ROOT_CONSTANTS(ProbeTraceRootConstants, rc)

[numthreads(DDGI_TRACE_SIZE, 1, 1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    RaytracingAccelerationStructure TLAS = ResourceDescriptorHeap[rc.mTLAS];
    StructuredBuffer<RTGeometry> geometries = ResourceDescriptorHeap[rc.mInstancesBuffer];
    StructuredBuffer<RTMaterial> materials  = ResourceDescriptorHeap[rc.mMaterialsBuffer];
    
    RWTexture2D<float> depth_texture       = ResourceDescriptorHeap[rc.mDDGIData.mRaysDepthTexture];
    RWTexture2D<float3> irradiance_texture = ResourceDescriptorHeap[rc.mDDGIData.mRaysIrradianceTexture];

    FrameConstants fc = gGetFrameConstants();

    uint ray_index = threadID.x;
    uint probe_index = threadID.y;
    uint2 ray_texture_index = uint2(ray_index, probe_index);
    
    float3 ray_dir = SphericalFibonnaci(ray_index, DDGI_RAYS_PER_PROBE);
    ray_dir = normalize(mul((float3x3)rc.mRandomRotationMatrix, ray_dir));
    
    float3 probe_ws_pos = DDGIGetProbeWorldPos(Index1DTo3D(probe_index, rc.mDDGIData.mProbeCount), rc.mDDGIData);
    
    RayDesc ray;
    ray.TMin = 0.01;
    ray.TMax = 10000.0;
    ray.Origin = probe_ws_pos;
    ray.Direction = ray_dir;

    RayQuery<RAY_FLAG_FORCE_OPAQUE> query;
    query.TraceRayInline(TLAS, RAY_FLAG_FORCE_OPAQUE, 0xFF, ray);
    query.Proceed();
    
    float hitT = length(rc.mDDGIData.mProbeSpacing);
    float3 irradiance = 0.xxx;

    if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        // TODO: write out the distance result here and terminate early right after if we hit a backface
        hitT = ray.TMin + query.CommittedRayT();
        
        RTGeometry geometry = geometries[query.CommittedInstanceID()];
        RTVertex vertex = CalculateVertexFromGeometry(geometry, query.CommittedPrimitiveIndex(), query.CommittedTriangleBarycentrics());
        TransformToWorldSpace(vertex, geometry.mLocalToWorldTransform, geometry.mInvLocalToWorldTransform);
        RTMaterial material = materials[geometry.mMaterialIndex];
        
        BRDF brdf;
        brdf.FromHit(vertex, material);
        //brdf.mNormal = vertex.mNormal; // use the vertex normal, texture based detail is lost anyway
        
        irradiance = material.mEmissive.rgb;
        
        const float3 Wo = -ray.Direction;
        const float3 Wi = normalize(-fc.mSunDirection.xyz);
        const float3 Wh = normalize(Wo + Wi);
        
        const float3 l = ((1.0 - brdf.mMetallic) * brdf.mAlbedo.rgb);
        

        const float NdotL = max(dot(brdf.mNormal, Wi), 0.0);
        float3 sunlight_luminance = Absorb(IntegrateOpticalDepth(0.xxx, -Wi)) * fc.mSunColor.a;
        
        irradiance += l * NdotL * sunlight_luminance;
        
        if (NdotL != 0.0)
        {
            RayDesc shadow_ray;
            shadow_ray.Origin = vertex.mPos + vertex.mNormal * 0.01;
            shadow_ray.Direction = Wi;
            shadow_ray.TMin = 0.0;
            shadow_ray.TMax = 1000.0;
        
            uint shadow_ray_flags = RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
    
            RayQuery < RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER > shadow_query;

            shadow_query.TraceRayInline(TLAS, shadow_ray_flags, 0xFF, shadow_ray);
            shadow_query.Proceed();
        
            irradiance *= shadow_query.CommittedStatus() != COMMITTED_TRIANGLE_HIT;
        }
        
        // TODO: make probe textures persistent, at this point in the rendergraph they don't exist yet
        // Infinite bounces!
        // irradiance += DDGISampleIrradiance(vertex.mPos, vertex.mNormal, rc.mDDGIData);
    }
    else
    {
        //float3 transmittance;
        float3 transmittance;
        float3 inscattering = IntegrateScattering(ray.Origin, -ray.Direction, 1.#INF, fc.mSunDirection.xyz, fc.mSunColor.rgb, transmittance);
        
        irradiance = max(inscattering, 0.0.xxx) * fc.mSunColor.a;
        
        hitT = ray.TMax;
    }
    
    if (probe_index == rc.mDebugProbeIndex) {
       // Resets indirect draw args VertexCount to 0 and InstanceCount to 1
       if (ray_index == 0)
           ResetDebugLineCount();
        
       float4 debug_ray_color = float4(irradiance, 1.0);
       float3 debug_ray_start = ray.Origin + ray.Direction * 0.25;
       float3 debug_ray_end   = ray.Origin + ray.Direction * hitT;
        
       // InterlockedAdd( 2 ) to the VertexCount, use the original value as write index into the line vertex buffer
       AddDebugLine(debug_ray_start, debug_ray_end, debug_ray_color, debug_ray_color);
    }
    
    
    depth_texture[ray_texture_index] = hitT;
    irradiance_texture[ray_texture_index] = irradiance;
    // irradiance_texture[ray_texture_index] = DDGIGetProbeDebugColor(probe_index, rc.mDDGIData.mProbeCount).rgb;
}
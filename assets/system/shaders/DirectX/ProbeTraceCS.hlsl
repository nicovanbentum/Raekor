#include "include/bindless.hlsli"
#include "include/packing.hlsli"
#include "include/common.hlsli"
#include "include/random.hlsli"
#include "include/ddgi.hlsli"
#include "include/brdf.hlsli"
#include "include/sky.hlsli"
#include "include/rt.hlsli"

ROOT_CONSTANTS(ProbeTraceRootConstants, rc)

[numthreads(1, 1, 1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    RaytracingAccelerationStructure TLAS = ResourceDescriptorHeap[rc.mTLAS];
    StructuredBuffer<RTGeometry> geometries = ResourceDescriptorHeap[rc.mInstancesBuffer];
    StructuredBuffer<RTMaterial> materials  = ResourceDescriptorHeap[rc.mMaterialsBuffer];
    
    RWTexture2D<float> depth_texture       = ResourceDescriptorHeap[rc.mRaysDepthTexture];
    RWTexture2D<float3> irradiance_texture = ResourceDescriptorHeap[rc.mRaysIrradianceTexture];
    
    FrameConstants fc = gGetFrameConstants();

    uint ray_index = threadID.x;
    uint probe_index = threadID.y;
    uint2 ray_texture_index = uint2(ray_index, probe_index);
    
    float3 ray_dir = SphericalFibonnaci(ray_index, DDGI_RAYS_PER_PROBE);
    float3 probe_ws_pos = GetDDGIProbeWorldPos(probe_index, rc.mProbeCount.xyz, rc.mBBmin.xyz, rc.mBBmax.xyz);
    
    
    RayDesc ray;
    ray.TMin = 0.01;
    ray.TMax = 10000.0;
    ray.Origin = probe_ws_pos;
    ray.Direction = ray_dir;

    RayQuery<RAY_FLAG_FORCE_OPAQUE> query;
    query.TraceRayInline(TLAS, RAY_FLAG_FORCE_OPAQUE, 0xFF, ray);
    query.Proceed();
    
    float depth = 10000.0;;
    float3 irradiance = 0.xxx;

    if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        // write out the linear depth (ray hit T) result here, we terminate early right after if we hit a backface
        depth = query.CommittedRayT();
        depth_texture[ray_texture_index] = depth;
        
        //if (!query.CommittedTriangleFrontFace()) {
        //    irradiance_texture[ray_texture_index] = irradiance;
        //    return;
        //}
        
        RTGeometry geometry = geometries[query.CommittedInstanceID()];
        RTVertex vertex = CalculateVertexFromGeometry(geometry, query.CommittedPrimitiveIndex(), query.CommittedTriangleBarycentrics());
        
        RTMaterial material = materials[geometry.mMaterialIndex];
        
        BRDF brdf;
        brdf.FromHit(vertex, material);
        
        // irradiance = brdf.mAlbedo.rgb;
        
        const float3 Wo = normalize(fc.mCameraPosition.xyz - vertex.mPos);
        const float3 Wi = normalize(-fc.mSunDirection.xyz);
        const float3 Wh = normalize(Wo + Wi);
        
        const float3 l = brdf.Evaluate(Wo, Wi, Wh);

        const float NdotL = max(dot(brdf.mNormal, Wi), 0.0);
        float3 sunlight_luminance = Absorb(IntegrateOpticalDepth(0.xxx, -Wi));
        irradiance = l * NdotL * sunlight_luminance;
        
        if (NdotL != 0.0) {
            RayDesc shadow_ray;
            shadow_ray.Origin = vertex.mPos + brdf.mNormal * 0.01;
            shadow_ray.Direction = Wi;
            shadow_ray.TMin = 0.1;
            shadow_ray.TMax = 1000.0;
        
            uint shadow_ray_flags = RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
    
            RayQuery < RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER > shadow_query;

            shadow_query.TraceRayInline(TLAS, shadow_ray_flags, 0xFF, shadow_ray);
            shadow_query.Proceed();
        
            //irradiance *= shadow_query.CommittedStatus() != COMMITTED_TRIANGLE_HIT;
        }
    }
    
    depth_texture[ray_texture_index] = depth;
    irradiance_texture[ray_texture_index] = irradiance;
}
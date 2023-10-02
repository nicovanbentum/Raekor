#include "include/bindless.hlsli"
#include "include/packing.hlsli"
#include "include/common.hlsli"
#include "include/random.hlsli"
#include "include/brdf.hlsli"
#include "include/sky.hlsli"

RTVertex InterpolateVertices(RTVertex v0, RTVertex v1, RTVertex v2, float3 inBaryCentrics) {
    RTVertex vertex;
    vertex.mPos = v0.mPos * inBaryCentrics.x + v1.mPos * inBaryCentrics.y + v2.mPos * inBaryCentrics.z;
    vertex.mTexCoord = v0.mTexCoord * inBaryCentrics.x + v1.mTexCoord * inBaryCentrics.y + v2.mTexCoord * inBaryCentrics.z;
    vertex.mNormal = v0.mNormal * inBaryCentrics.x + v1.mNormal * inBaryCentrics.y + v2.mNormal * inBaryCentrics.z;
    vertex.mTangent = v0.mTangent * inBaryCentrics.x + v1.mTangent * inBaryCentrics.y + v2.mTangent * inBaryCentrics.z;
    
    return vertex;
}

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


    float depth = gbuffer_depth_texture[threadID.xy];
    if (depth >= 1.0) {
        result_texture[threadID.xy] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    const uint4 gbuffer_sample = asuint(gbuffer_texture[threadID.xy]);
    
    const float3 normal = UnpackNormal(gbuffer_sample);
    const float4 gbuffer_albedo = UnpackAlbedo(gbuffer_sample);
    
    float metallic, roughness;
    UnpackMetallicRoughness(gbuffer_sample, metallic, roughness);
    
    if (roughness >= 0.3) {
        result_texture[threadID.xy] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    
    const float3 position = ReconstructWorldPosition(screen_uv, depth, fc.mInvViewProjectionMatrix);
    
    const float3 Wo = normalize(fc.mCameraPosition.xyz - position.xyz);
    const float3 Wi = normalize(reflect(Wo, normal));
    
    RayDesc ray;
    ray.TMin = 0.1;
    ray.TMax = 1000.0;
    ray.Origin = position + normal * 0.01; // TODO: find a more robust method? offsetRay from ray tracing gems 
                                            // expects a geometric normal, which most deferred renderers dont write out
    ray.Direction = -Wi;

    uint ray_flags = RAY_FLAG_FORCE_OPAQUE;
    
    RayQuery < RAY_FLAG_FORCE_OPAQUE > query;

    query.TraceRayInline(TLAS, ray_flags, 0xFF, ray);
    query.Proceed();
    

    if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        RTGeometry geometry = geometries[query.CommittedInstanceID()];
        
        StructuredBuffer<uint3> index_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(geometry.mIndexBuffer)];
        StructuredBuffer<RTVertex> vertex_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(geometry.mVertexBuffer)];
        
        const uint3 indices = index_buffer[query.CommittedPrimitiveIndex()];
        const RTVertex v0 = vertex_buffer[indices.x];
        const RTVertex v1 = vertex_buffer[indices.y];
        const RTVertex v2 = vertex_buffer[indices.z];
        
        float3 vert_normal = normalize(cross(v0.mPos - v1.mPos, v0.mPos - v2.mPos));
        
        float2 hit_bary = query.CommittedTriangleBarycentrics();
        const float3 barycentrics = float3(1.0 - hit_bary.x - hit_bary.y, hit_bary.x, hit_bary.y);
        RTVertex vertex = InterpolateVertices(v0, v1, v2, barycentrics);
        TransformToWorldSpace(vertex, geometry.mLocalToWorldTransform, geometry.mInvLocalToWorldTransform);
        
        RTMaterial material = materials[geometry.mMaterialIndex];
        
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
            shadow_ray.Direction.xz += uniformSampleDisk(blue_noise.xy, 0.05);
            
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
        result_texture[threadID.xy] = 0.xxxx;
    }
    
    //result_texture[threadID.xy] = float4(normal, 1.0);
}
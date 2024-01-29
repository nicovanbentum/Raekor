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
    RWTexture2D<float4> result_texture = ResourceDescriptorHeap[rc.mResultTexture];
    
    RaytracingAccelerationStructure TLAS = ResourceDescriptorHeap[rc.mTLAS];
    StructuredBuffer<RTGeometry> geometries = ResourceDescriptorHeap[rc.mInstancesBuffer];
    StructuredBuffer<RTMaterial> materials  = ResourceDescriptorHeap[rc.mMaterialsBuffer];
    
    const FrameConstants fc = gGetFrameConstants();

    uint rng = TeaHash(((threadID.y << 16) | threadID.x), fc.mFrameCounter + 1);
    
    const float2 pixel_center = float2(threadID.xy) + float2(0.5, 0.5);
    float2 screen_uv = pixel_center / rc.mDispatchSize;

    float depth = gbuffer_depth_texture[threadID.xy];
    if (depth >= 1.0) 
    {
        result_texture[threadID.xy] = 0.xxxx;
        return;
    }

    BRDF gbuffer_brdf;
    gbuffer_brdf.Unpack(asuint(gbuffer_texture[threadID.xy]));

    if (gbuffer_brdf.mRoughness >= 0.3 && gbuffer_brdf.mMetallic < 0.1) 
    {
        result_texture[threadID.xy] = 0.xxxx;
        return;
    }
    
    const float3 position = ReconstructWorldPosition(screen_uv, depth, fc.mInvViewProjectionMatrix);
    
    const float3 Wo = normalize(fc.mCameraPosition.xyz - position.xyz);
    
    float3 Wi, Wh;
    gbuffer_brdf.SampleSpecular(rng, Wo, Wi, Wh);
    //const float3 Wi = normalize(reflect(-Wo, gbuffer_brdf.mNormal.xyz));
    
    RayDesc ray;
    ray.TMin = 0.1;
    ray.TMax = 1000.0;
    ray.Origin = position + gbuffer_brdf.mNormal * 0.01; // TODO: find a more robust method?
    ray.Direction = Wi;

    RayQuery < RAY_FLAG_FORCE_OPAQUE > query;
    query.TraceRayInline(TLAS, RAY_FLAG_FORCE_OPAQUE, 0xFF, ray);
    query.Proceed();
    
    float3 irradiance = 0.xxx;

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
            
        const float3 Wo = normalize(position - vertex.mPos);
        const float3 Wh = normalize(Wo + Wi);
        
        BRDF brdf;
        brdf.FromHit(vertex, material);
        
        float3 Wi = SampleDirectionalLight(fc.mSunDirection.xyz, 0.0f, 0.xx);

        const float NdotL = max(dot(brdf.mNormal, Wi), 0.0);

        if (NdotL > 0.0) 
        {
            bool hit = TraceShadowRay(TLAS, vertex.mPos + vertex.mNormal * 0.01, Wi, 0.1f, 1000.0f);
            
            if (!hit)
                irradiance += EvaluateDirectionalLight(brdf, fc.mSunColor, Wi, Wo);
                //irradiance += brdf.mAlbedo.rgb * NdotL * fc.mSunColor.a;
        }
    }
    else
    {
        // Calculate sky
        float3 transmittance;
        float3 inscattering = IntegrateScattering(ray.Origin, -ray.Direction, 1.#INF, fc.mSunDirection.xyz, fc.mSunColor.rgb, transmittance);
            
        irradiance += max(inscattering, 0.0.xxx) * fc.mSunColor.a;
    }
    
    result_texture[threadID.xy] = float4(irradiance, 1.0);
}
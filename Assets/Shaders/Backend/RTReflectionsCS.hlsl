#include "Include/Bindless.hlsli"
#include "Include/Packing.hlsli"
#include "Include/Sky.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"
#include "Include/Material.hlsli"
#include "Include/DDGI.hlsli"
#include "Include/RayTracing.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(ReflectionsRootConstants, rc)

#define MAX_REFLECTION_DISTANCE 1000.0f

[numthreads(8,8,1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    if (any(threadID.xy >= rc.mDispatchSize.xy))
        return;

    Texture2D<float4> gbuffer_texture = ResourceDescriptorHeap[rc.mGbufferRenderTexture];
    Texture2D<float> gbuffer_depth_texture = ResourceDescriptorHeap[rc.mGbufferDepthTexture];
    RWTexture2D<float4> result_texture = ResourceDescriptorHeap[rc.mResultTexture];

    RaytracingAccelerationStructure TLAS = ResourceDescriptorHeap[fc.mTLAS];
    StructuredBuffer<RTGeometry> geometries = ResourceDescriptorHeap[fc.mInstancesBuffer];
    StructuredBuffer<RTMaterial> materials  = ResourceDescriptorHeap[fc.mMaterialsBuffer];

    TextureCube<float3> skycube_texture = ResourceDescriptorHeap[rc.mSkyCubeTexture];
    TextureCube<float3> diffuse_skycube_texture = ResourceDescriptorHeap[rc.mDiffuseSkyCubeTexture];

    const float2 pixel_center = float2(threadID.xy) + float2(0.5, 0.5);
    float2 screen_uv = pixel_center / rc.mDispatchSize;

    float depth = gbuffer_depth_texture[threadID.xy];
    if (depth >= 1.0)
    {
        result_texture[threadID.xy] = float4(0.0, 0.0, 0.0, MAX_REFLECTION_DISTANCE);
        return;
    }

    Surface surface;
    surface.Unpack(asuint(gbuffer_texture[threadID.xy]));

    const float3 position = ReconstructWorldPosition(screen_uv, depth, fc.mInvViewProjectionMatrix);
    const float3 Wo = normalize(fc.mCameraPosition.xyz - position.xyz);

    RayDesc ray;
    ray.TMin = 0.0;
    ray.TMax = MAX_REFLECTION_DISTANCE;
    ray.Origin = position + surface.mNormal * 0.01;
    ray.Direction = reflect(-Wo, surface.mNormal);

    RayQuery < RAY_FLAG_FORCE_OPAQUE > query;
    query.TraceRayInline(TLAS, RAY_FLAG_FORCE_OPAQUE, 0xFF, ray);
    while (query.Proceed()) {}

    float3 radiance = 0.xxx;
    float hit_distance = MAX_REFLECTION_DISTANCE;

    if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        hit_distance = query.CommittedRayT();

        RTGeometry geometry = geometries[query.CommittedInstanceID()];
        RTVertex vertex = CalculateVertexFromGeometry(geometry, query.CommittedPrimitiveIndex(), query.CommittedTriangleBarycentrics());
        RTMaterial material = materials[geometry.mMaterialIndex];

        TransformToWorldSpace(vertex, geometry.mWorldTransform);
        vertex.mNormal = normalize(vertex.mNormal);

        if (dot(vertex.mNormal, ray.Direction) > 0.0)
            vertex.mNormal = -vertex.mNormal;

        Surface hit_surface;
        hit_surface.FromHit(vertex, material);
        hit_surface.mNormal = vertex.mNormal;

        const float3 hit_Wo = -ray.Direction;
        const float3 sun_Wi = normalize(-fc.mSunDirection.xyz);

        radiance = hit_surface.mEmissive;

        if (dot(hit_surface.mNormal, sun_Wi) > 0.0 && !TraceShadowRay(TLAS, vertex.mPos + vertex.mNormal * 0.01, sun_Wi, 0.0f, MAX_REFLECTION_DISTANCE))
            radiance += EvaluateDirectionalLight(hit_surface, fc.mSunColor, sun_Wi, hit_Wo);

        float3 indirect_diffuse = rc.mUseDDGI ?
            DDGISampleIrradiance(vertex.mPos, hit_surface.mNormal, hit_Wo, rc.mDDGIData) :
            diffuse_skycube_texture.SampleLevel(SamplerLinearClamp, hit_surface.mNormal, 0) * fc.mSunColor.a;

        radiance += indirect_diffuse * hit_surface.mAlbedo.rgb * (1.0 - hit_surface.mMetallic);
    }
    else
    {
        radiance = max(skycube_texture.SampleLevel(SamplerLinearClamp, ray.Direction, 0), 0.0.xxx) * fc.mSunColor.a;
    }

    result_texture[threadID.xy] = float4(radiance, hit_distance);
}

#include "Include/Bindless.hlsli"
#include "Include/Packing.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"
#include "Include/Material.hlsli"
#include "Include/Sky.hlsli"
#include "Include/RayTracing.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(PathTraceRootConstants, rc)

static const float cMaxIndirectLuminance = 10.0;


bool PassesAlphaTest(uint inInstanceID, uint inPrimitiveIndex, float2 inBarycentrics)
{
    StructuredBuffer<RTGeometry> geometries = ResourceDescriptorHeap[fc.mInstancesBuffer];
    StructuredBuffer<RTMaterial> materials = ResourceDescriptorHeap[fc.mMaterialsBuffer];

    const RTGeometry geometry = geometries[inInstanceID];
    const RTMaterial material = materials[geometry.mMaterialIndex];
    const RTVertex vertex = CalculateVertexFromGeometry(geometry, inPrimitiveIndex, inBarycentrics);

    Texture2D albedo_texture = ResourceDescriptorHeap[NonUniformResourceIndex(material.mAlbedoTexture)];
    const float alpha = material.mAlbedo.a * albedo_texture.SampleLevel(SamplerPointWrapNoMips, vertex.mTexCoord, 0).a;

    return alpha >= material.mAlphaCutoff;
}


bool IsOccluded(RaytracingAccelerationStructure inTLAS, float3 inOrigin, float3 inDirection, float inTMax)
{
    RayDesc ray;
    ray.Origin = inOrigin;
    ray.Direction = inDirection;
    ray.TMin = 0.0;
    ray.TMax = inTMax;

    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;
    query.TraceRayInline(inTLAS, RAY_FLAG_NONE, 0xFF, ray);

    while (query.Proceed())
    {
        if (query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE && PassesAlphaTest(query.CandidateInstanceID(), query.CandidatePrimitiveIndex(), query.CandidateTriangleBarycentrics()))
            query.CommitNonOpaqueTriangleHit();
    }

    return query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}


float3 SampleLight(Surface inSurface, float3 inPosition, float3 inGeometricNormal, float3 Wo, inout uint ioRNG)
{
    RaytracingAccelerationStructure TLAS = ResourceDescriptorHeap[fc.mTLAS];
    RaytracingAccelerationStructure shadow_TLAS = ResourceDescriptorHeap[fc.mShadowTLAS];
    StructuredBuffer<RTLight> lights = ResourceDescriptorHeap[fc.mLightsBuffer];

    const float3 shadow_origin = OffsetRay(inPosition, inGeometricNormal);

    float3 radiance = 0.0.xxx;

    if (fc.mSunColor.a > 0.0)
    {
        const float3 Wi = SampleDirectionalLight(fc.mSunDirection.xyz, fc.mSunConeAngle, pcg_float2(ioRNG));

        if (dot(inGeometricNormal, Wi) > 0.0 && dot(inSurface.mNormal, Wi) > 0.0 && !IsOccluded(shadow_TLAS, shadow_origin, Wi, 10000.0))
            radiance += EvaluateDirectionalLight(inSurface, fc.mSunColor, Wi, Wo);
    }

    if (fc.mNrOfLights == 0)
        return radiance;

    const uint light_index = min(uint(pcg_float(ioRNG) * fc.mNrOfLights), fc.mNrOfLights - 1);
    const RTLight light = lights[light_index];

    if (light.mType != RT_LIGHT_TYPE_POINT && light.mType != RT_LIGHT_TYPE_SPOT)
        return radiance;

    const float3 to_light = light.mPosition.xyz - inPosition;
    const float distance = length(to_light);
    const float3 Wi = to_light / max(distance, 1e-4);

    if (dot(inGeometricNormal, Wi) <= 0.0 || dot(inSurface.mNormal, Wi) <= 0.0)
        return radiance;

    if (IsOccluded(TLAS, shadow_origin, Wi, distance))
        return radiance;

    const float3 light_radiance = light.mType == RT_LIGHT_TYPE_POINT ? EvaluatePointLight(inSurface, light, Wi, Wo, distance) : EvaluateSpotLight(inSurface, light, Wi, Wo, distance);

    return radiance + light_radiance * float(fc.mNrOfLights);
}


[numthreads(8,8,1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    if (any(threadID.xy >= rc.mDispatchSize.xy))
        return;

    RWTexture2D<float> depth_texture         = ResourceDescriptorHeap[rc.mDepthTexture];
    RWTexture2D<float4> result_texture       = ResourceDescriptorHeap[rc.mResultTexture];
    RWTexture2D<uint4> gbuffer_texture       = ResourceDescriptorHeap[rc.mGBufferTexture];
    TextureCube<float3> skycube_texture      = ResourceDescriptorHeap[rc.mSkyCubeTexture];
    RWTexture2D<uint> selection_texture      = ResourceDescriptorHeap[rc.mSelectionTexture];
    RWTexture2D<float4> accumulation_texture = ResourceDescriptorHeap[rc.mAccumulationTexture];

    RaytracingAccelerationStructure TLAS        = ResourceDescriptorHeap[fc.mTLAS];
    StructuredBuffer<RTGeometry> geometries     = ResourceDescriptorHeap[fc.mInstancesBuffer];
    StructuredBuffer<RTMaterial> materials      = ResourceDescriptorHeap[fc.mMaterialsBuffer];

    const bool is_first_sample = rc.mReset || fc.mFrameCounter < 2;

    uint rng = TeaHash(((threadID.y << 16) | threadID.x), fc.mFrameCounter + 1);

    const float2 pixel_offset = is_first_sample ? 0.5.xx : pcg_float2(rng);
    const float2 screen_uv = (float2(threadID.xy) + pixel_offset) / rc.mDispatchSize;
    const float2 clip = float2(screen_uv.x * 2.0 - 1.0, (1.0 - screen_uv.y) * 2.0 - 1.0);

    const float4 target = mul(fc.mInvViewProjectionMatrix, float4(clip, 0.5, 1.0));

    RayDesc ray;
    ray.TMin = 0.0;
    ray.TMax = 10000.0;
    ray.Origin = fc.mCameraPosition.xyz;
    ray.Direction = normalize(target.xyz / target.w - ray.Origin);

    uint entity = 0;
    float depth = 1.0;
    uint4 gbuffer = 0.xxxx;

    float3 radiance = 0.0.xxx;
    float3 throughput = 1.0.xxx;

    for (uint bounce = 0; bounce < rc.mBounces; bounce++)
    {
        RayQuery<RAY_FLAG_NONE> query;
        query.TraceRayInline(TLAS, RAY_FLAG_NONE, 0xFF, ray);

        while (query.Proceed())
        {
            if (query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE && PassesAlphaTest(query.CandidateInstanceID(), query.CandidatePrimitiveIndex(), query.CandidateTriangleBarycentrics()))
                query.CommitNonOpaqueTriangleHit();
        }

        float3 contribution = 0.0.xxx;

        if (query.CommittedStatus() != COMMITTED_TRIANGLE_HIT)
        {
            contribution = throughput * max(skycube_texture.SampleLevel(SamplerLinearClamp, ray.Direction, 0), 0.0.xxx) * fc.mSunColor.a;

            if (bounce > 0)
                contribution *= min(1.0, cMaxIndirectLuminance / max(LuminanceLinear(contribution), 1e-6));

            radiance += contribution;
            break;
        }

        const RTGeometry geometry = geometries[query.CommittedInstanceID()];
        const RTMaterial material = materials[geometry.mMaterialIndex];

        RTVertex vertex = CalculateVertexFromGeometry(geometry, query.CommittedPrimitiveIndex(), query.CommittedTriangleBarycentrics());
        TransformToWorldSpace(vertex, geometry.mWorldTransform);

        float3 geometric_normal = CalculateGeometricNormal(geometry, query.CommittedPrimitiveIndex());

        if (dot(geometric_normal, ray.Direction) > 0.0)
        {
            geometric_normal = -geometric_normal;
            vertex.mNormal = -vertex.mNormal;
        }

        Surface surface;
        surface.FromHit(vertex, material, true);

        if (bounce == 0 && is_first_sample)
        {
            const float4 clip_pos = mul(fc.mViewProjectionMatrix, float4(vertex.mPos, 1.0));
            depth = clip_pos.z / clip_pos.w;
            entity = geometry.mEntity;
            PackGBuffer(surface.mAlbedo, surface.mNormal, surface.mEmissive, surface.mMetallic, surface.mRoughness, gbuffer);
        }

        const float3 Wo = -ray.Direction;

        contribution = throughput * (surface.mEmissive + SampleLight(surface, vertex.mPos, geometric_normal, Wo, rng));

        if (bounce > 0)
            contribution *= min(1.0, cMaxIndirectLuminance / max(LuminanceLinear(contribution), 1e-6));

        radiance += contribution;

        float3 brdf_weight;
        surface.SampleBRDF(rng, Wo, ray.Direction, brdf_weight);

        if (dot(ray.Direction, geometric_normal) <= 0.0 || all(brdf_weight <= 0.0))
            break;

        throughput *= brdf_weight;
        ray.Origin = OffsetRay(vertex.mPos, geometric_normal);

        if (bounce >= 3)
        {
            const float survival_probability = min(max(throughput.r, max(throughput.g, throughput.b)), 0.95);

            if (pcg_float(rng) >= survival_probability)
                break;

            throughput /= survival_probability;
        }
    }

    if (any(isnan(radiance)) || any(isinf(radiance)))
        radiance = 0.0.xxx;

    if (is_first_sample)
    {
        depth_texture[threadID.xy] = depth;
        gbuffer_texture[threadID.xy] = gbuffer;
        selection_texture[threadID.xy] = entity;

        result_texture[threadID.xy] = float4(radiance, 1.0);
        accumulation_texture[threadID.xy] = float4(radiance, 1.0);
    }
    else
    {
        float4 accumulation = accumulation_texture[threadID.xy] + float4(radiance, 1.0);

        result_texture[threadID.xy] = float4(accumulation.rgb / accumulation.a, 1.0);
        accumulation_texture[threadID.xy] = accumulation;
    }
}

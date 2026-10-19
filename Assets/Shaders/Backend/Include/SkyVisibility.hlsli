#ifndef SKY_VISIBILITY_HLSLI
#define SKY_VISIBILITY_HLSLI

#define SKY_VISIBILITY_PROBE_HEIGHT 0.6
#define SKY_VISIBILITY_MIN_SPAN 4.0
#define SKY_VISIBILITY_NORMAL_OFFSET 1.0

float2 GetSkyMapUV(float4 inBounds, float3 inPosWS, float3 inNormal)
{
    return (inPosWS.xz + inNormal.xz * SKY_VISIBILITY_NORMAL_OFFSET - inBounds.xy) * inBounds.zw;
}


float SampleSkyVisibility(uint inTexture, float4 inBounds, float4 inParams, float3 inPosWS, float3 inNormal)
{
    Texture2D<float4> sky_visibility_texture = ResourceDescriptorHeap[NonUniformResourceIndex(inTexture)];

    const float4 texel = sky_visibility_texture.SampleLevel(SamplerLinearClamp, GetSkyMapUV(inBounds, inPosWS, inNormal), 0);

    const float ground = inParams.x + texel.g * inParams.y;
    const float top = inParams.x + texel.b * inParams.y;
    const float above = saturate((inPosWS.y - ground - SKY_VISIBILITY_PROBE_HEIGHT) / max(top - ground, SKY_VISIBILITY_MIN_SPAN));

    return lerp(1.0, lerp(texel.r, 1.0, above), inParams.z);
}


float3 SampleGroundAlbedo(uint inTexture, float4 inBounds, float3 inPosWS, float3 inNormal)
{
    Texture2D<float4> ground_albedo_texture = ResourceDescriptorHeap[NonUniformResourceIndex(inTexture)];
    return ground_albedo_texture.SampleLevel(SamplerLinearClamp, GetSkyMapUV(inBounds, inPosWS, inNormal), 0).rgb;
}


float3 EvaluateGroundBounce(float3 inNormal, float3 inGroundAlbedo, float4 inSunColor, float3 inSunDirection, float4 inGroundBounce)
{
    if (inGroundBounce.a <= 0.0)
        return 0.0.xxx;

    const float3 Wi = normalize(-inSunDirection);
    const float3 sun_irradiance = Absorb(IntegrateOpticalDepth(0.xxx, -Wi)) * inSunColor.a * saturate(Wi.y);
    const float ground_view = saturate(0.5 - 0.5 * inNormal.y);

    return inGroundBounce.rgb * inGroundAlbedo * inGroundBounce.a * sun_irradiance * ground_view / M_PI;
}


float3 EvaluateGroundRadiance(float3 inGroundAlbedo, float3 inSkyLight, float4 inSunColor, float3 inSunDirection)
{
    const float3 Wi = normalize(-inSunDirection);
    const float3 sun_irradiance = Absorb(IntegrateOpticalDepth(0.xxx, -Wi)) * inSunColor.a * saturate(Wi.y);

    return inGroundAlbedo * (sun_irradiance / M_PI + inSkyLight);
}


float3 BlendGroundReflection(float3 inSky, float3 inGround, float3 inDirection, float inRoughness)
{
    const float width = 0.03 + inRoughness * 0.6;
    return lerp(inGround, inSky, smoothstep(-width, width * 0.25, inDirection.y));
}

#endif // SKY_VISIBILITY_HLSLI

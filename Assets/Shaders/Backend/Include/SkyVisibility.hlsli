#ifndef SKY_VISIBILITY_HLSLI
#define SKY_VISIBILITY_HLSLI

#define SKY_VISIBILITY_PROBE_HEIGHT 0.6
#define SKY_VISIBILITY_MIN_SPAN 4.0

float SampleSkyVisibility(uint inTexture, float4 inBounds, float4 inParams, float3 inPosWS)
{
    if (inParams.w <= 0.0)
        return 1.0;

    Texture2D<float4> sky_visibility_texture = ResourceDescriptorHeap[NonUniformResourceIndex(inTexture)];

    const float2 uv = (inPosWS.xz - inBounds.xy) * inBounds.zw;
    const float4 texel = sky_visibility_texture.SampleLevel(SamplerLinearClamp, uv, 0);

    const float ground = inParams.x + texel.g * inParams.y;
    const float top = inParams.x + texel.b * inParams.y;
    const float above = saturate((inPosWS.y - ground - SKY_VISIBILITY_PROBE_HEIGHT) / max(top - ground, SKY_VISIBILITY_MIN_SPAN));

    return lerp(1.0, lerp(texel.r, 1.0, above), inParams.z);
}


float3 EvaluateGroundBounce(float3 inNormal, float4 inSunColor, float3 inSunDirection, float4 inGroundBounce)
{
    if (inGroundBounce.a <= 0.0)
        return 0.0.xxx;

    const float3 Wi = normalize(-inSunDirection);
    const float3 sun_irradiance = Absorb(IntegrateOpticalDepth(0.xxx, Wi)) * inSunColor.a * saturate(Wi.y);
    const float ground_view = saturate(0.5 - 0.5 * inNormal.y);

    return inGroundBounce.rgb * inGroundBounce.a * sun_irradiance * ground_view / M_PI;
}

#endif // SKY_VISIBILITY_HLSLI

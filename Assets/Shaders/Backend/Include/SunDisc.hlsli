#ifndef SUN_DISC_HLSLI
#define SUN_DISC_HLSLI

#define SUN_DISC_EDGE_SOFTNESS 0.15
#define SUN_DISC_LIMB_DARKENING 0.6
#define SUN_DISC_MAX_RADIANCE 64.0

float3 EvaluateSunDisc(float3 inViewDirection, float4 inSunColor, float3 inSunDirection, float4 inSunDisc)
{
    if (inSunDisc.y <= 0.0)
        return 0.0.xxx;

    const float3 Wi = normalize(-inSunDirection);
    const float cos_theta = dot(inViewDirection, Wi);
    const float radius = inSunDisc.z;

    if (cos_theta < cos(radius * ( 1.0 + SUN_DISC_EDGE_SOFTNESS )))
        return 0.0.xxx;

    const float theta = acos(saturate(cos_theta)) / radius;
    const float edge = 1.0 - smoothstep(1.0 - SUN_DISC_EDGE_SOFTNESS, 1.0 + SUN_DISC_EDGE_SOFTNESS, theta);
    const float limb = 1.0 - SUN_DISC_LIMB_DARKENING * ( 1.0 - sqrt(saturate(1.0 - theta * theta)) );
    const float horizon = smoothstep(-0.002, 0.002, inViewDirection.y);

    const float3 sun_radiance = Absorb(IntegrateOpticalDepth(0.xxx, -Wi)) * inSunColor.a / inSunDisc.y;
    const float peak = max(sun_radiance.r, max(sun_radiance.g, sun_radiance.b));
    const float3 capped_radiance = sun_radiance * min(1.0, SUN_DISC_MAX_RADIANCE / max(peak, 1e-6));

    return capped_radiance * edge * limb * horizon;
}

#endif // SUN_DISC_HLSLI

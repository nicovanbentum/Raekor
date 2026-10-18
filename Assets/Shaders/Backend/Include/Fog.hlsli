#ifndef FOG_HLSLI
#define FOG_HLSLI

#define FOG_MIN_HORIZON_HEIGHT 0.01

float3 GetFogColor(TextureCube<float3> inSkyCube, float3 inViewDirection, float inExposure)
{
    float3 direction = normalize(inViewDirection);
    direction = normalize(float3(direction.x, max(direction.y, FOG_MIN_HORIZON_HEIGHT), direction.z));
    return max(inSkyCube.SampleLevel(SamplerLinearClamp, direction, 0), 0.0.xxx) * inExposure;
}


float GetFogAmount(float4 inFogParams, float inDistance)
{
    return smoothstep(inFogParams.x, inFogParams.y, inDistance) * inFogParams.z;
}


float3 ApplyFog(float3 inRadiance, TextureCube<float3> inSkyCube, float4 inFogParams, float3 inViewDirection, float inDistance, float inExposure)
{
    if (inFogParams.z <= 0.0)
        return inRadiance;

    return lerp(inRadiance, GetFogColor(inSkyCube, inViewDirection, inExposure), GetFogAmount(inFogParams, inDistance));
}

#endif // FOG_HLSLI

#ifndef DDGI_HLSLI
#define DDGI_HLSLI

#include "common.hlsli"

uint GetDDGIProbeIndex(uint3 inProbeCoord, uint3 inProbeCount) {
    return inProbeCoord.x +
           inProbeCoord.y * inProbeCount.x +
           inProbeCoord.z * inProbeCount.x * inProbeCount.y;
}

uint3 GetDDGIProbeCoordinate(uint i, uint3 inProbeCount) {
    return uint3(
        i % inProbeCount.x,
        (i / inProbeCount.x) % inProbeCount.y,
        i / (inProbeCount.x * inProbeCount.y)
    );
}

float3 GetDDGIProbeWorldPos(uint inProbeIndex, uint3 inProbeCount, float3 inBBmin, float3 inBBmax) {
    float3 probe_offset = (abs(inBBmin) + inBBmax).xyz / inProbeCount;
    float3 probe_coord = float3(GetDDGIProbeCoordinate(inProbeIndex, inProbeCount));
    return inBBmin.xyz + probe_coord * probe_offset;
}

/*
    Octahedral and spherical fibonacci projections code from Ray Tracing Gems 2 Chapter 3
*/
float SignNotZero(float inValue) {
    return (inValue >= 0.0) ? 1.0 : -1.0;
}

float2 SignNotZero(float2 inVec) {
    return float2(SignNotZero(inVec.x), SignNotZero(inVec.y));
}

float2 OctEncode(float3 inVec) {
    float l1norm = abs(inVec.x) + abs(inVec.y) + abs(inVec.z);
    float2 result = inVec.xy * (1.0 / l1norm);
    
    if (inVec.z < 0.0)
        result = (1.0 - abs(result.yx)) * SignNotZero(result.xy);
    
    return result;
}

float3 OctDecode(float2 inOct) {
    float3 v = float3(inOct.x, inOct.y, 1.0 - abs(inOct.x) - abs(inOct.y));
    if (v.z < 0.0)
        v.xy = (1.0 - abs(v.yx)) * SignNotZero(v.xy);
    
    return normalize(v);
}

float3 SphericalFibonnaci(uint i, uint n) {
    const float PHI = sqrt(5) * 0.5 + 0.5;
    float fraction = (i * (PHI - 1)) - floor(i * (PHI - 1));
    float phi = 2.0 * M_PI * fraction;
    float cos_theta = 1.0 - (2.0 * i + 1.0) * (1.0 / n);
    float sin_theta = sqrt(saturate(1.0 - cos_theta * cos_theta));
    return normalize(float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta));
}

#endif // DDGI_HLSLI
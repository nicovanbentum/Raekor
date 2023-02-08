#include "include/bindless.hlsli"
#include "include/packing.hlsli"
#include "include/common.hlsli"
#include "include/random.hlsli"

ROOT_CONSTANTS(ProbeTraceRootConstants, rc)

/* octahedral unit vector mapping from https://jcgt.org/published/0003/02/01/paper.pdf */
float2 signNotZero(float2 v) {
    return float2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
}

// Assume normalized input. Output is on [-1, 1] for each component.
float2 float32x3_to_oct(in float3 v) {
// Project the sphere onto the octahedron, and then onto the xy plane
    float2 p = v.xy * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
// Reflect the folds of the lower hemisphere over the diagonals
    return (v.z <= 0.0) ? ((1.0 - abs(p.yx)) * signNotZero(p)) : p;
}

float3 oct_to_float32x3(float2 e) {
    float3 v = float3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    if (v.z < 0)
        v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
    return normalize(v);
}

#define madfrac(A,B) mad((A), (B), -floor((A)*(B)))

float3 SphericalFibonnaci(float i, float n) {
    float phi = 2 * asfloat(0x40490fdc) * madfrac(i, (sqrt(5) * 0.5 + 0.5) - 1);
    float cosTheta = 1 - (2 * i + 1) * rcp(n);
    float sinTheta = sqrt(saturate(1 - cosTheta * cosTheta));
    return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

[numthreads(1, DDGI_WAVE_SIZE, 1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    RaytracingAccelerationStructure TLAS = ResourceDescriptorHeap[rc.mTLAS];
    
    FrameConstants fc = gGetFrameConstants();

    uint probe_index = threadID.x;
    uint ray_index = threadID.y;
    
    RayDesc ray;
    ray.TMin = 0.1;
    ray.TMax = 1000.0;
    ray.Origin = rc.mProbeWorldPos.xyz;
    ray.Direction = normalize(SphericalFibonnaci(ray_index, rc.mDispatchSize.y));

    uint ray_flags =  RAY_FLAG_FORCE_OPAQUE |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
    
    RayQuery< RAY_FLAG_FORCE_OPAQUE |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;

    query.TraceRayInline(TLAS, ray_flags, 0xFF, ray);
    query.Proceed();

    bool missed = query.CommittedStatus() != COMMITTED_TRIANGLE_HIT;
}
#ifndef RANDOM_HLSLI
#define RANDOM_HLSLI

#define M_PI 3.14159265358979323846

// From Ray Tracing Gems chapter 6
float3 offsetRay(float3 p, float3 n) {
    const float origin = 1.0 / 32.0;
    const float float_scale = 1.0 / 65536.0;
    const float int_scale = 256.0;

    int3 of_i = int3(int_scale * n.x, int_scale * n.y, int_scale * n.z);

    float3 p_i = float3(
        asfloat(asint(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
        asfloat(asint(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
        asfloat(asint(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));

    return float3(abs(p.x) < origin ? p.x + float_scale * n.x : p_i.x,
                  abs(p.y) < origin ? p.y + float_scale * n.y : p_i.y,
                  abs(p.z) < origin ? p.z + float_scale * n.z : p_i.z);
}

float3 cosineWeightedSampleHemisphere(float2 rng) {
    const float theta = 6.2831853 * rng.x;
    const float u = 2.0 * rng.y - 1.0;
    const float r = sqrt(1.0 - u * u);
    return float3(r * cos(theta), r * sin(theta), u);
}

float2 uniformSampleDisk(float2 u) {
    float r = sqrt(u.x);
    float theta = 2 * M_PI * u.y;
    return float2(r * cos(theta), r * sin(theta));
}

float2 uniformSampleDisk(float2 u, float radius) {
    float r = radius * sqrt(u.x);
    float theta = 2 * M_PI * u.y;
    return float2(r * cos(theta), r * sin(theta));
}

float3 uniformSampleCone(const float2 u, float cosThetaMax) {
    float cosTheta = (1.0 - u.x) + u.x * cosThetaMax;
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    float phi = u.y * 2 * M_PI;
    return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

// from https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
uint pcg_hash(inout uint in_state)
{
    uint state = in_state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    in_state = (word >> 22u) ^ word;
    return in_state;
}

float pcg_float(inout uint in_state) {
    return pcg_hash(in_state) / float(uint(0xffffffff));
}

float2 pcg_float2(inout uint in_state) {
    return float2(pcg_float(in_state), pcg_float(in_state));
}

float3 pcg_float3(inout uint in_state) {
    return float3(pcg_float(in_state), pcg_float(in_state), pcg_float(in_state));
}


#endif // RANDOM_HLSLI
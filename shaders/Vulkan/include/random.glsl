#ifndef RANDOM_GLSL
#define RANDOM_GLSL

#define M_PI 3.14159265358979323846

// From Ray Tracing Gems chapter 6
vec3 offsetRay(vec3 p, vec3 n) {
    const float origin = 1.0f / 32.0f;
    const float float_scale = 1.0f / 65536.0f;
    const float int_scale = 256.0f;

    ivec3 of_i = ivec3(int_scale * n.x, int_scale * n.y, int_scale * n.z);

    vec3 p_i = vec3(
        intBitsToFloat(floatBitsToInt(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
        intBitsToFloat(floatBitsToInt(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
        intBitsToFloat(floatBitsToInt(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));

    return vec3(abs(p.x) < origin ? p.x + float_scale * n.x : p_i.x,
        abs(p.y) < origin ? p.y + float_scale * n.y : p_i.y,
        abs(p.z) < origin ? p.z + float_scale * n.z : p_i.z);
}

vec3 cosineWeightedSampleHemisphere(vec2 rng) {
    const float theta = 6.2831853 * rng.x;
    const float u = 2.0 * rng.y - 1.0;
    const float r = sqrt(1.0 - u * u);
    return vec3(r * cos(theta), r * sin(theta), u);
}

vec2 uniformSampleDisk(vec2 u) {
    float r = sqrt(u.x);
    float theta = 2 * M_PI * u.y;
    return vec2(r * cos(theta), r * sin(theta));
}

vec2 uniformSampleDisk(vec2 u, float radius) {
    float r = radius * sqrt(u.x);
    float theta = 2 * M_PI * u.y;
    return vec2(r * cos(theta), r * sin(theta));
}

vec3 uniformSampleCone(const vec2 u, float cosThetaMax) {
    float cosTheta = (1.0 - u.x) + u.x * cosThetaMax;
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    float phi = u.y * 2 * M_PI;
    return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
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

vec2 pcg_vec2(inout uint in_state) {
    return vec2(pcg_float(in_state), pcg_float(in_state));
}

vec3 pcg_vec3(inout uint in_state) {
    return vec3(pcg_float(in_state), pcg_float(in_state), pcg_float(in_state));
}


#endif
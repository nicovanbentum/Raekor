#ifndef RANDOM_GLSL
#define RANDOM_GLSL

#define M_PI 3.14159265358979323846

vec3 uniformSampleSphere(vec2 u) {
    float z = 1 - 2 * u.x;
    float r = sqrt(max(0, 1 - z * z));
    float phi = 2 * M_PI * u.y;
    return vec3(r * cos(phi), r * sin(phi), z);
}

vec3 uniformSampleHemisphere(vec2 u) {
    float z = u.x;
    float r = sqrt(max(0.0, 1.0 - z * z));
    float phi = 2 * M_PI * u.y;
    return vec3(r * cos(phi), r * sin(phi), z);
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
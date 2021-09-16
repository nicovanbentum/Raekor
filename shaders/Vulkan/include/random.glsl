#ifndef RANDOM_GLSL
#define RANDOM_GLSL

uvec4 seed ( in vec2 fragCoord, in vec2 iResolution, uint frameCounter) {
    float fseed = fragCoord.x + fragCoord.y * iResolution.x + 1.0 + (1.0 / frameCounter) * iResolution.x * iResolution.y;
    return uvec4(fseed);
}

// Noise generation from https://www.shadertoy.com/view/MtycWc
// https://merlin3d.wordpress.com/2018/10/04/correlated-multi-jittered-sampling-on-gpu/
// Generate an unsigned integer random vector in the range of [0..range-1]^3 
// with uniform distribution from a linear seed using mixing functions.
// Maximum valid value of range is 65535.
#define RANGE uvec4(65535u)
#define MAX_RANDOM float(RANGE-1u)

vec4 rand(uvec4 s) {
    s ^= uvec4(s.x >> 19, s.y >> 19, s.z >> 13, s.w >> 19);
    s *= uvec4(1619700021, 3466831627, 923620057, 3466831627);
    s ^= uvec4(s.x >> 16, s.y >> 12, s.z >> 18, s.w >> 17);
    s *= uvec4(175973783, 2388179301, 3077236438, 2388179301);
    s ^= uvec4(s.x >> 15, s.y >> 17, s.z >> 18, s.w >> 18);
    
    uvec4 f = s >> 16;
    uvec4 random = (RANGE * f + (s % RANGE)) >> 16;    
    return random / MAX_RANDOM;
}

vec2 UniformSampleDisk(vec2 u) {
    float r = sqrt(u.x);
    float theta = 2 * 3.1415 * u.y;
    return vec2(r * cos(theta), r * sin(theta));
}

vec3 UniformSampleCone(const vec2 u, float cosThetaMax) {
    float cosTheta = (1.0 - u.x) + u.x * cosThetaMax;
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    float phi = u.y * 2 * 3.1415;
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


#endif
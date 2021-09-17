#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "include/structs.glsl"
#include "include/sky.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;

vec3 sky(vec3 direction) {
    float t = 0.5 * (normalize(direction).y + 1.0);
    return mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);
}

void main() {
    vec3 rayDir = normalize(-gl_WorldRayDirectionEXT);
    vec3 rayStart = gl_WorldRayOriginEXT;
    float rayLength = INFINITY;

    vec3 lightDir = normalize(vec3(-0.1, -1, -0.2));

    vec3 transmittance;
    vec3 color = IntegrateScattering(rayStart, rayDir, rayLength, lightDir, vec3(1.0), transmittance);


    payload.Lo += 0.1 * sky(gl_WorldRayDirectionEXT);
}
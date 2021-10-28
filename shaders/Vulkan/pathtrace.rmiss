#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "include/structs.glsl"
#include "include/sky.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;

layout(push_constant) uniform pushConstants {
    mat4 invViewProj;
    vec4 cameraPosition;
    vec4 lightDir;
    uint frameCounter;
    uint bounces;
    float sunConeAngle;
};

vec3 sky(vec3 direction) {
    float t = 0.5 * (normalize(direction).y + 1.0);
    return mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);
}

void main() {
    vec3 rayDir = normalize(-gl_WorldRayDirectionEXT);
    vec3 rayStart = gl_WorldRayOriginEXT;
    float rayLength = INFINITY;

    vec3 transmittance;
    vec3 color = IntegrateScattering(rayStart, rayDir, rayLength, lightDir.xyz, vec3(1.0), transmittance);

    if(payload.depth == 0) {
        payload.L = color;
    } else {
        payload.L = vec3(0.01);
    }

    payload.depth = bounces + 1; // terminate
}
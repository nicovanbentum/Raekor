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

void main() {
    vec3 rayDir = normalize(-gl_WorldRayDirectionEXT);
    vec3 rayStart = gl_WorldRayOriginEXT;
    float rayLength = INFINITY;

    vec3 transmittance;
    vec3 color = IntegrateScattering(rayStart, rayDir, rayLength, lightDir.xyz, vec3(1.0), transmittance);

    const float y = normalize(gl_WorldRayDirectionEXT).y;

    if(y > 0.0) {
        // set L to throughput * sky color so it gets added
        payload.L = mix(vec3(1.0), vec3(0.25, 0.5, 1.0), y);
    } else {
        payload.L = vec3(0.03);
    }

    payload.L = min(color, vec3(1.0)) * lightDir.w;
    payload.depth = bounces + 10; // terminate
    

    // payload.L = vec3(0); // temp
}
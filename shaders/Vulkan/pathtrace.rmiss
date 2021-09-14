#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "include/structs.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main() {
    payload.Lio = vec3(0.5, 0.7, 1.0);
}
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

layout(location = 1) rayPayloadInEXT bool canReachLight;

void main() {
    canReachLight = true;
}


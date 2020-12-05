#version 460

#extension GL_NV_ray_tracing : require

layout(location = 0) rayPayloadInNV vec3 payload;

void main() {
    // if this shader executes it means it never hit any geometry on its way to the light
    payload = vec3(1);
}
#version 460

#extension GL_NV_ray_tracing : require

layout(location = 0) rayPayloadInNV vec3 payload;

void main() {
    payload = vec3(gl_HitTNV / 100.0f);
}
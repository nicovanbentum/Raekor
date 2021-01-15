#version 460

#extension GL_NV_ray_tracing : require

layout(location = 0) rayPayloadInNV vec3 payload;

float map(float value, float min1, float max1, float min2, float max2) {
  return min2 + (value - min1) * (max2 - min2) / (max1 - min1);
}

void main() {
    payload = vec3(gl_HitTNV);
}
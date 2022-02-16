#version 450

layout(location = 0) out vec4 final_color;

layout(location = 0) in vec3 localPos;

layout(location = 0) uniform sampler2D equirectMap;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() {
    vec2 uv = SampleSphericalMap(normalize(localPos)); // make sure to normalize localPos
    final_color = vec4(texture(equirectMap, uv).rgb, 1.0);
}

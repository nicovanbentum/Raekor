#ifndef STRUCTS_GLSL
#define STRUCTS_GLSL

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

struct Vertex {
    vec3 pos;
    vec2 uv;
    vec3 normal;
    vec3 tangent;
};

struct Material {
    vec4 albedo;
    ivec4 textures; // x = albedo, y = normals, z = metalroughness
    vec4 properties; // x = metallic, y = roughness, z = emissive
};

struct Instance {
    ivec4 materialIndex; // x is the index
    mat4 localToWorldTransform;
    uint64_t indexBufferDeviceAddress;
    uint64_t vertexBufferDeviceAddress;
};

struct RayPayload {
    vec3 L;
    vec3 beta;
    vec3 rayPos;
    vec3 rayDir;
    uint rng;
    uint depth;
};

struct Surface {
    vec3 pos;
    vec3 normal;
    vec4 albedo;
    float metallic, roughness;
};

#endif
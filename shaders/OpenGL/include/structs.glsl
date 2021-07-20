#ifndef STRUCTS_GLSL
#define STRUCTS_GLSL

struct Material {
    vec4 colour;
    float metallic, roughness;
    float pad0, pad1;
    int albedo;
    int normals;
    int metalrough;
};

struct Transform {
    vec4 position;
    vec4 rotation;
    vec4 scale;
    mat4 local;
    mat4 world;
};

#endif
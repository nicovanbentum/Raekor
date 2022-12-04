#version 440 core

struct Vertex {
    float pos[3];
    float uv[2];
    float normal[3];
    float tangent[3];
};

layout(std430, binding = 1) buffer VertexBuffer {
    Vertex vertices[];
};

layout(binding = 0) uniform ubo {
    mat4 px, py, pz;
    mat4 shadowMatrices[4];
    mat4 view;
    mat4 model;
    vec4 shadowSplits;
    vec4 colour;
};

layout(location = 0) out vec2 uvs;
layout(location = 1) out vec4 worldPositions;

void main() {
    Vertex v = vertices[gl_VertexID];
    vec3 v_pos = vec3(v.pos[0], v.pos[1], v.pos[2]);

    worldPositions = model * vec4(v_pos ,1);
    gl_Position = worldPositions;

    uvs = vec2(v.uv[0], v.uv[1]);
}
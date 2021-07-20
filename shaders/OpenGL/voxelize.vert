#version 440 core

layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_normal;

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
    worldPositions = model * vec4(v_pos ,1);
    gl_Position = worldPositions;
    uvs = v_uv;
}
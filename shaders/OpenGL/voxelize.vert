#version 440 core

layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_normal;

uniform mat4 model;

out vec2 uvs;
out vec4 worldPositions;

void main() {
    worldPositions = model * vec4(v_pos ,1);
    gl_Position = worldPositions;
    uvs = v_uv;
}
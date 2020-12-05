#version 440 core

layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_normal;

uniform mat4 model;

out vec2 uvs;

void main() {
    gl_Position = model * vec4(v_pos ,1);
    uvs = v_uv;
}
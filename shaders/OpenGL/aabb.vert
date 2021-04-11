#version 440 core
layout (location = 0) in vec3 v_pos;

uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * vec4(v_pos, 1.0);
}
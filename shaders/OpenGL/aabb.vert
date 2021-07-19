#version 440 core
layout (location = 0) in vec3 v_pos;

layout(binding = 0) uniform ubo {
    mat4 projection;
    mat4 view;
};

void main() {
    gl_Position = projection * view * vec4(v_pos, 1.0);
}
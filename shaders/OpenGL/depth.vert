#version 460 core
layout (location = 0) in vec3 pos;

layout(binding = 0) uniform ubo {
    mat4 lightMatrix;
    mat4 modelMatrix;
};

void main() {
    gl_Position = lightMatrix * modelMatrix * vec4(pos, 1.0);
} 
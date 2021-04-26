#version 330 core
layout (location = 0) in vec3 pos;

uniform mat4 model;
uniform mat4 lightMatrix;

void main() {
    gl_Position = lightMatrix * model * vec4(pos, 1.0);
} 
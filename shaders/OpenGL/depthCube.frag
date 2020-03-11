#version 440 core

in vec3 fragPos;

uniform vec3 lightPos;
uniform float farPlane;

void main() {
    gl_FragDepth = length(lightPos - fragPos) / 25.0f; 
} 
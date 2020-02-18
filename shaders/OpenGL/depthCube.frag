#version 440

in vec3 fragPos;

uniform vec3 lightPos;
uniform float farPlane;

out float fragDepth;
out vec4 fragColor;

void main() {
    gl_FragDepth = length(lightPos - fragPos) / 25.0; 
    fragColor = vec4(1.0);
} 
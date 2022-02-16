#version 440 core

layout(location = 0) in vec3 fragPos;

layout (binding = 0, std140) uniform Uniforms {
    mat4 model;
	mat4 faceMatrices[6];
	vec4 lightPos;
    mat4 projView;
	float farPlane;
	float x, y, z;
};

void main() {
    gl_FragDepth = length(lightPos.xyz - fragPos) / 25.0f; 
} 
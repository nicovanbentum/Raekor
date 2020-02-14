#version 440 core
layout (location = 0) in vec3 pos;

layout (std140) uniform stuff {
    mat4 model;
	mat4 faceMatrices[6];
	vec4 lightPos;
	float farPlane;
	float x, y, z;
} ubo;

void main() {
    gl_Position = ubo.model * vec4(pos, 1.0);
} 
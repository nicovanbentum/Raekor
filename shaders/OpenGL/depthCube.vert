#version 440 core
layout (location = 0) in vec3 pos;

layout(location = 0) out vec3 fragPos;

layout (binding = 0, std140) uniform Uniforms {
    mat4 model;
	mat4 faceMatrices[6];
	vec4 lightPos;
    mat4 projView;
	float farPlane;
	float x, y, z;
};

void main() {
	gl_Position = projView * model * vec4(pos, 1.0);
	fragPos = vec3(model * vec4(pos, 1.0));
} 
#version 440 core
out vec4 color;

layout (std140) uniform stuff {
    mat4 model, view, projection;
	mat4 lightSpaceMatrix;
	vec4 cameraPosition;
    vec4 DirLightPos;
	vec4 pointLightPos;
} ubo;

void main() {
    color = vec4(0.9, 0.9, 0.9, 1.0);
}
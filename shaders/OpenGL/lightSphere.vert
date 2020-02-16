#version 440 core
layout (location = 0) in vec3 pos;

layout (std140) uniform stuff {
    mat4 model, view, projection;
	mat4 lightSpaceMatrix;
	vec4 cameraPosition;
    vec4 DirLightPos;
	vec4 pointLightPos;
} ubo;

void main()
{
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(pos, 1.0);
}  
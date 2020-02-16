#version 330 core
layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec3 normal;

out vec2 TexCoords;

layout (std140) uniform stuff {
	float exposure;
	float gamma;
} ubo;

void main()
{
	TexCoords = uv;
    gl_Position = vec4(pos, 1.0);
}
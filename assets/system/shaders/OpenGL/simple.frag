#version 440 core
layout(location = 0) out vec4 color;

layout(location = 0) in vec3 texturePos;

layout(binding = 0) uniform samplerCube skybox;

void main()
{    
    color = texture(skybox, texturePos);
	//color = vec4(texture_pos.xyz, 1.0f); // debug colors
}
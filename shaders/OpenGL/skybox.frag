#version 440 core
out vec4 color;

in vec3 texture_pos;

layout(binding = 0) uniform samplerCube skybox;

void main()
{    
    color = texture(skybox, texture_pos);
	//color = vec4(texture_pos.xyz, 1.0f); // debug colors
}
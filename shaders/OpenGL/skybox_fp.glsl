#version 330 core
out vec4 color;

in vec3 texture_pos;

uniform samplerCube skybox;

void main()
{    
    color = texture(skybox, texture_pos);
	//color = vec4(texture_pos.xyz, 1.0f); // debug colors
}
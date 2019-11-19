#version 330 core
out vec4 color;

in vec3 texture_pos;

uniform samplerCube skybox;

void main()
{    
    color = texture(skybox, texture_pos);
}
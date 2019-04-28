#version 330 core

//blender uvs
in vec2 UV;

//output data back to our openGL program
out vec3 color;

//constant mesh values
uniform sampler2D myTextureSampler;

void main()
{
	color = texture(myTextureSampler, UV).bgr;
}
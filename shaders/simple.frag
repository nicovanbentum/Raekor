#version 330 core

// blender uvs
in vec2 uv;

// output data back to our openGL program
out vec4 color;

// constant mesh values
uniform sampler2D sampler;

void main()
{
    color = texture(sampler, uv);
}
#version 330 core
layout (location = 0) in vec3 pos;

out vec3 texture_pos;

uniform mat4 MVP;

void main()
{
    texture_pos = pos;
    gl_Position = MVP * vec4(pos, 1.0);
}  
#version 330 core
layout (location = 0) in vec3 pos;

out vec3 texture_pos;

uniform mat4 view;
uniform mat4 proj;

void main()
{
    vec4 glPos = proj * view * vec4(pos, 1.0);
	gl_Position = glPos.xyww;
    texture_pos = pos;
}  
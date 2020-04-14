#version 330 core
layout (location = 0) in vec3 pos;

out vec3 texturePos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

void main()
{
    vec3 worldPos = vec3(model * vec4(pos, 1.0));
    gl_Position = proj * view * vec4(worldPos, 1.0);
    texturePos = pos;
}  
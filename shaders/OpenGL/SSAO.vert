#version 440 core

layout (location = 0) in vec3 pos;

uniform float aspectRatio;
uniform float tanHalfFOV;

out vec2 uv;
out vec2 ray;

void main()
{
    gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);
    uv = (pos.xy + vec2(1.0)) / 2.0;
    ray.x = pos.x * aspectRatio * tanHalfFOV;
    ray.y = pos.y * tanHalfFOV;
}
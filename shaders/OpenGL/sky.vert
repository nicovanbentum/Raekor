#version 440 core

layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_uv;

uniform mat4 projection;
uniform mat4 view;
uniform float time = 0.0;

out vec3 pos;
out vec3 fsun;

void main() {
    gl_Position = vec4(v_pos.x, v_pos.y, 0.0, 1.0);
    pos = transpose(mat3(view)) * (inverse(projection) * gl_Position).xyz;
    pos.x += 0;
    fsun = vec3(0.0, sin(time * 0.01), cos(time * 0.01));
}
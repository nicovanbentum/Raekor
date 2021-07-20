#version 440 core

layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_uv;

layout(binding = 0) uniform Uniforms {
    mat4 projection;
    mat4 view;
    float time;
    float cirrus; // = 0.4;
    float cumulus; // = 0.8;
};

layout(location = 0) out vec3 pos;
layout(location = 1) out vec3 fsun;

void main() {
    gl_Position = vec4(v_pos.x, v_pos.y, 0.0, 1.0);
    pos = transpose(mat3(view)) * (inverse(projection) * gl_Position).xyz;
    pos.x += 0;
    fsun = vec3(0.0, sin(time * 0.01), cos(time * 0.01));
}
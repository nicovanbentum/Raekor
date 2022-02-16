#version 440 core

layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_normal;

layout(binding = 0) uniform Uniforms {
    mat4 projection;
    mat4 view;
    mat4 model;
};

layout(location = 0) out vec3 pos;

void main() {
	pos = (model * vec4(v_pos, 1.0)).xyz;
	gl_Position = projection * view * vec4(pos, 1.0);
}
#version 440 core

layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_normal;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

out vec3 pos;

void main() {
	pos = (model * vec4(v_pos, 1.0)).xyz;
	gl_Position = projection * view * vec4(pos, 1.0);
}
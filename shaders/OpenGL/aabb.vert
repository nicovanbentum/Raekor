#version 440 core
layout (location = 0) in vec3 v_pos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
	vec3 pos = (model * vec4(v_pos, 1.0)).xyz;
    gl_Position = projection * view * vec4(pos, 1.0);
}
#version 440 core

// vertex buffer data
layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_normal;
layout(location = 3) in vec3 v_tangent;
layout(location = 4) in vec3 v_binormal;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

out vec3 pos;
out vec2 uv;

void main() {
	pos = vec3(model * vec4(v_pos, 1.0));
	gl_Position = projection * view * vec4(pos, 1.0);
	
	uv = v_uv;
}
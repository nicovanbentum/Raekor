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
out vec3 normal;
out mat3 TBN;

void main() {
	pos = vec3(model * vec4(v_pos, 1.0));
	gl_Position = projection * view * vec4(pos, 1.0);

	vec3 T = normalize(vec3(model * vec4(v_tangent,		0.0)));
	vec3 B = normalize(vec3(model * vec4(v_binormal,	0.0)));
	vec3 N = normalize(vec3(model * vec4(v_normal,		0.0)));
	TBN = mat3(T, B, N);
	
	uv = v_uv;
	normal = v_normal;
}
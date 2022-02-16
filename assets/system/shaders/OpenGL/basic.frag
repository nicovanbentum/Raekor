#version 440 core

layout(location = 0) out vec4 final_color;

layout(location = 0) in vec3 pos;

void main() {
	final_color = vec4(pos, 1.0);
}
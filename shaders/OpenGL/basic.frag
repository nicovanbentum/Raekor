#version 440 core

out vec4 final_color;

in vec3 pos;

void main() {
	final_color = vec4(pos, 1.0);
}
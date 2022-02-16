#version 440 core

layout(location = 0) in vec4 color;
layout(location = 1) in flat uint instance;

layout(location = 0) out vec4 final_color;

void main() {
	if(color.a < 0.5f) {
		discard;
    }

	final_color = vec4(color);
    final_color.a = 1.0;
}
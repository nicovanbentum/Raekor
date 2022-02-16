#version 440 core

layout(location = 0) in vec4 fragColor;

layout(location = 0) out vec4 final_colour;

void main() {
	if(fragColor.a < 0.5f) {
		discard;
    }

	final_colour = vec4(fragColor);
    final_colour.a = 1.0;
}
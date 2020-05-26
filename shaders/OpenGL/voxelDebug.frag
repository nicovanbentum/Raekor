#version 440 core

in vec3 normal;
in vec4 fragColor;

out vec4 final_colour;

void main() {
	if(fragColor.a < 0.5f)
		discard;

	final_colour = vec4(fragColor);
}
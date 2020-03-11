#version 440 core
layout (location = 0) in vec3 pos;

uniform mat4 model; 
uniform mat4 projView;

out vec3 fragPos;

void main() {
	gl_Position = projView * model * vec4(pos, 1.0);
	fragPos = vec3(model * vec4(pos, 1.0));
} 
#version 440
layout (location = 0) in vec3 pos;

uniform mat4 model, view, projection;

out vec4 FragPos;

void main() {
	FragPos = model * vec4(pos, 1.0);
	gl_Position = projection * view * FragPos;
} 
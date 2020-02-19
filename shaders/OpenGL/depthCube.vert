#version 440 core
layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 uv;

uniform mat4 model; 
uniform mat4 projView;

out vec3 fragPos;
out vec2 texCoord;

void main() {
	gl_Position = projView * model * vec4(pos, 1.0);
	fragPos = vec3(model * vec4(pos, 1.0));

	texCoord = uv;

} 
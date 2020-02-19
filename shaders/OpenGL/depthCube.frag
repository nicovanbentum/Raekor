#version 440 core
out vec4 color;

in vec3 fragPos;
in vec2 texCoord;

uniform vec3 lightPos;
uniform float farPlane;

// constant mesh values
layout(binding = 0) uniform sampler2D meshTexture;

void main() {
    gl_FragDepth = length(lightPos - fragPos) / 25.0f; 
    color = texture(meshTexture, texCoord);
	//color = vec4(1.0, 1.0, 0.0, 1.0);
} 
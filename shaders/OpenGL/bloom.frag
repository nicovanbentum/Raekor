#version 440 core
layout(location = 0) out vec4 FragColor;

layout(location = 0) in vec2 TexCoords;

layout(binding = 0) uniform sampler2D sceneTexture;
layout(binding = 1) uniform sampler2D bloomTexture;

void main() {             
    vec3 hdrColor = texture(sceneTexture, TexCoords).rgb;      
    vec3 bloomColor = texture(bloomTexture, TexCoords).rgb;
    FragColor = vec4(hdrColor + bloomColor, 1.0);
}  
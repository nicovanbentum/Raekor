#version 440 core

out vec4 color;
  
in vec2 TexCoords;
  
layout(binding = 0) uniform sampler2D ssaoTexture;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(ssaoTexture, 0));
    float result = 0.0;
    for (int x = -2; x < 2; ++x) {
        for (int y = -2; y < 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(ssaoTexture, TexCoords + offset).r;
        }
    }
    float aoFactor = result / (4.0 * 4.0);
	color = vec4(aoFactor, aoFactor, aoFactor, 1.0);
} 
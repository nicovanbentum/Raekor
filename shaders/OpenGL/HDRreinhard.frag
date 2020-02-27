#version 440 core
out vec4 FragColor;
  
in vec2 TexCoords;

layout(binding = 0) uniform sampler2D hdrBuffer;

layout (std140) uniform stuff {
	float exposure;
	float gamma;
} ubo;

void main()
{             
    vec3 hdrColor = texture(hdrBuffer, TexCoords).rgb;
	hdrColor = hdrColor / (1+hdrColor);
    vec3 result = pow(hdrColor, vec3(1.0 / ubo.gamma));
    FragColor = vec4(result, 1.0);
} 
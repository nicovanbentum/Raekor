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
	// reinhard
    // vec3 result = hdrColor / (hdrColor + vec3(1.0));
    // exposure
    vec3 result = vec3(1.0) - exp(-hdrColor * ubo.exposure);
    // also gamma correct while we're at it       
    result = pow(result, vec3(1.0 / ubo.gamma));
    FragColor = vec4(result, 1.0);
} 
#version 440 core
out vec4 FragColor;
  
layout(location = 0) in vec2 TexCoords;

layout(binding = 0) uniform sampler2D hdrBuffer;
layout(binding = 1) uniform sampler2D bloom;

layout (std140) uniform stuff {
	float exposure;
	float gamma;
} ubo;

float A = 0.15;
float B = 0.50;
float C = 0.10;
float D = 0.20;
float E = 0.02;
float F = 0.30;
float W = 11.2;

vec3 Uncharted2Tonemap(vec3 x)
{
   return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

void main()
{             
    vec3 hdrColor = texture(hdrBuffer, TexCoords).rgb;
    hdrColor += texture(bloom, TexCoords).rgb;

	float exposureBias = 2.0f;
	vec3 curr = Uncharted2Tonemap(exposureBias*hdrColor);

	vec3 whiteScale = 1.0f / Uncharted2Tonemap(vec3(W));
	vec3 color = curr*whiteScale;

	vec3 retColor = pow(color, vec3(1/ubo.gamma));
	FragColor = vec4(retColor, 1.0);
} 
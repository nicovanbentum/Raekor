#version 440 core

layout(location = 0) out vec4 FragColor;
  
layout(location = 0) in vec2 TexCoords;

layout(binding = 0) uniform sampler2D depthMap;

float LinearizeDepth(float depth)
{
  float n = 1.0; // camera z near
  float f = 128.0; // camera z far
  float z = depth;
  return (2.0 * n) / (f + n - z * (f - n));	
}

void main()
{ 
    float depthValue = texture(depthMap, TexCoords).r;
    FragColor = vec4(vec3(depthValue), 1.0);
}
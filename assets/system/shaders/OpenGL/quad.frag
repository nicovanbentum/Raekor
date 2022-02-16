#version 440 core

layout(location = 0) out vec4 FragColor;
  
layout(location = 0) in vec2 TexCoords;

layout(binding = 0) uniform sampler2DArrayShadow depthMap;

float LinearizeDepth(float depth)
{
  float n = 0.1;
  float f = 10000.0;
  float z = depth;
  return (2.0 * n) / (f + n - z * (f - n));	
}

void main()
{ 
    float depthValue = texture(depthMap, vec4(TexCoords, 0, 1.0)).r;
    FragColor = vec4(vec3(depthValue), 1.0);
}
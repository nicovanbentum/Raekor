#version 440 core
in vec4 FragPos;

layout (std140) uniform stuff {
    mat4 model;
	mat4 faceMatrices[6];
	vec4 lightPos;
	float farPlane;
	float x, y, z;
} ubo;

void main()
{
    // get distance between fragment and light source
    float lightDistance = length(FragPos.xyz - ubo.lightPos.xyz);
    
    // map to [0;1] range by dividing by far_plane
    lightDistance = lightDistance / ubo.farPlane;
    
    // write this as modified depth
    gl_FragDepth = lightDistance;
} 
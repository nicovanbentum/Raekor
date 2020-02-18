#version 440

in vec4 FragPos;

uniform float farPlane;
uniform vec3 lightPos;

void main()
{
    float lightDistance = length(FragPos.xyz - lightPos);
    
    // map to [0;1] range by dividing by far_plane
    lightDistance = lightDistance / farPlane;
    
    gl_FragDepth = lightDistance;
} 
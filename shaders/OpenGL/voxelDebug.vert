#version 440 core

layout (location = 0) in vec3 pos;

out vec2 TexCoords;

// Scales and bias a given vector (i.e. from [-1, 1] to [0, 1]).
vec2 scaleAndBias(vec2 p) { return 0.5f * p + vec2(0.5f); }

void main()
{
    gl_Position = vec4(pos, 1.0);
    TexCoords = scaleAndBias(pos.xy);
}
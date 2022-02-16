#version 440 core
layout (location = 0) in vec3 pos;

layout(location = 0) out vec3 texturePos;

layout(binding = 0) uniform Uniforms {
    mat4 model;
    mat4 view;
    mat4 proj;
};

void main()
{
    vec3 worldPos = vec3(model * vec4(pos, 1.0));
    gl_Position = proj * view * vec4(worldPos, 1.0);
    texturePos = pos;
}  
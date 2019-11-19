#version 450
#extension GL_KHR_vulkan_glsl: enable
#extension GL_ARB_separate_shader_objects : enable

// in variables
layout(location = 0) in vec3 texture_pos;

// out variables
layout(location = 0) out vec4 color;

layout(set = 0, binding = 1) uniform samplerCube skybox;

void main()
{    
    color = texture(skybox, texture_pos);
}
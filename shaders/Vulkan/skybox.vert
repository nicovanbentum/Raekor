#version 450
#extension GL_KHR_vulkan_glsl: enable
#extension GL_ARB_separate_shader_objects : enable

// vertex attributes
layout(location = 0) in vec3 pos;

layout(location = 0) out vec3 texture_pos;

// uniform buffer for the MVP
layout (binding = 0) uniform Camera {
	mat4 mvp;
} ubo;

// out variables

void main()
{
    texture_pos = pos;
	gl_Position = ubo.mvp * vec4(pos, 1.0);
}  
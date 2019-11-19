#version 450
#extension GL_KHR_vulkan_glsl: enable
#extension GL_ARB_separate_shader_objects : enable

// vertex attributes
layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 uv;

// uniform buffer for the MVP
layout (binding = 0) uniform Camera {
	mat4 model_view_projection;
};

// out variables
layout(location = 0) out vec3 texture_pos;

void main()
{
    texture_pos = pos;
    gl_Position = model_view_projection * vec4(pos, 1.0);
}  
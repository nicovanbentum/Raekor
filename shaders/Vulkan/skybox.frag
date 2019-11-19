#version 450
#extension GL_KHR_vulkan_glsl: enable
#extension GL_ARB_separate_shader_objects : enable

// out variables
layout(location = 0) out vec4 final_color;

// in variables
layout(location = 0) in vec3 texture_pos;

layout(set = 0, binding = 1) uniform samplerCube skybox;

void main()
{    
    final_color = texture(skybox, texture_pos);
	// debug colours
	//final_color = vec4(texture_pos.x, texture_pos.y, texture_pos.z, 1.0f);
}
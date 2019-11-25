#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_vulkan_glsl : enable

// in vars
layout(location = 0) in vec4 color;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 normal;

// in uniforms
layout(set = 0, binding = 1) uniform sampler2D tex_sampler[24]; 
layout(push_constant) uniform pushConstants {
	int samplerIndex;
} pc;

// out vars
layout(location = 0) out vec4 final_color;



void main() {
    final_color = texture(tex_sampler[pc.samplerIndex], uv);
}
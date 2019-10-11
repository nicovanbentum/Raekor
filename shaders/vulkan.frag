#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 color;
layout(location = 0) out vec4 final_color;

layout(binding = 1) uniform sampler2D tex_sampler; 

layout(location = 1) in vec2 uv;

void main() {
    final_color = texture(tex_sampler, uv);
}
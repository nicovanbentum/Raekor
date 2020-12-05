#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 texcoord;
layout(location = 2) in vec3 normal;

layout(location = 0) out vec3 fragColor;

layout(binding = 0) uniform Uniforms {
    mat4 view;
    mat4 projection;
} uniforms;

layout(push_constant) uniform pushConstants {
    mat4 model;
} pc;

void main()
{
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
	gl_Position = uniforms.projection * uniforms.view * worldPos;
	fragColor = worldPos.rgb;
}
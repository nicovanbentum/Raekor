#version 450
#extension GL_KHR_vulkan_glsl: enable
#extension GL_ARB_separate_shader_objects : enable

// shader uniform buffer
struct MVP {
	mat4 m;
	mat4 v;
	mat4 p;
	vec3 light_position;
};

// uniform buffer binding
layout(binding = 0) uniform Camera {
    MVP mvp;
} ubo;

// vertex attributes
layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 normal;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec3 out_normal;
layout(location = 3) out vec3 out_pos;
layout(location = 4) out vec3 out_light_pos;

// DEBUG COLORS
vec3 colors[4] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(0.0, 0.0, 1.0)
);

void main() {
	// CAMERA SPACE : VERTEX POSITION
    gl_Position = ubo.mvp.p * ubo.mvp.v * ubo.mvp.m * vec4(pos, 1.0);
	out_pos = vec3(ubo.mvp.v * ubo.mvp.m * vec4(pos, 1.0));
	out_normal = mat3(transpose(inverse(ubo.mvp.v * ubo.mvp.m))) * normal;
	out_light_pos = vec3(ubo.mvp.v * vec4(ubo.mvp.light_position, 1.0));
	out_uv = uv;

	// DEBUG COLOR INDEX
    int index = gl_VertexIndex;
    clamp(index, 0, 4);

    out_color = vec4(colors[index], 1.0);
}
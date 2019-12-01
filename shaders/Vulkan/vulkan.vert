#version 450
#extension GL_KHR_vulkan_glsl: enable
#extension GL_ARB_separate_shader_objects : enable

struct MVP {
	mat4 m;
	mat4 v;
	mat4 p;
	vec3 lightPos;
	vec3 viewPos;
};

layout(binding = 0) uniform Camera {
    MVP mvp;
} ubo;

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 normal;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec3 out_normal;
layout(location = 3) out vec3 out_direction;
layout(location = 4) out vec3 out_light_direction;
layout(location = 5) out vec3 out_pos;
layout(location = 6) out vec3 out_light_position;


vec3 colors[4] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(0.0, 0.0, 1.0)
);

void main() {
    vec3 worldPos = vec3(ubo.mvp.m * vec4(pos, 1.0));
    gl_Position = ubo.mvp.p * ubo.mvp.v * vec4(worldPos, 1.0);

    // Position of the vertex, in worldspace : M * position
	out_pos = (ubo.mvp.m * vec4(pos,1)).xyz;
    vec3 vertexPosition_cameraspace = ( ubo.mvp.v * ubo.mvp.m * vec4(pos,1)).xyz;
	out_direction = vec3(0,0,0) - vertexPosition_cameraspace;

    // Vector that goes from the vertex to the light, in camera space. M is ommited because it's identity.
	vec3 LightPosition_cameraspace = ( ubo.mvp.v * vec4(ubo.mvp.lightPos,1)).xyz;
	out_light_direction = ubo.mvp.lightPos + out_direction;
	
	// Normal of the the vertex, in camera space
	out_normal = ( ubo.mvp.v * ubo.mvp.m * vec4(normal,0)).xyz; // Only correct if ModelMatrix does not scale the model ! Use its inverse transpose if not.

    int index = gl_VertexIndex;
    clamp(index, 0, 4);
    out_color = vec4(colors[index], 1.0);

	out_uv = uv;
	out_light_position = ubo.mvp.lightPos;
}
#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_vulkan_glsl : enable

// in vars
layout(location = 0) in vec4 color;
layout(location = 1) in vec2 uv;
// normal, direction and light direction in camera space
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 direction;
layout(location = 4) in vec3 light_direction;
// gl position in world space
layout(location = 5) in vec3 position;
// light position in world space
layout(location = 6) in vec3 light_pos;

// in uniforms
layout(set = 0, binding = 1) uniform sampler2D tex_sampler[24]; 
layout(push_constant) uniform pushConstants {
	int samplerIndex;
} pc;

// out vars
layout(location = 0) out vec4 final_color;

void main() {
	// make these dynamic
	float constant = 1.0f;
	float linear = 0.09f;
	float quadratic = 0.032f;
	vec3 light_color = vec3(1.0, 0.7725, 0.56);
	float light_power = 50.0f;

    // Material properties
	vec3 diffuse_color = texture( tex_sampler[pc.samplerIndex], uv ).rgb;
	vec3 ambient_color = vec3(0.1, 0.1, 0.1) * diffuse_color;

    // Distance to the light
	float distance = length(light_pos - position);
	float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));
	ambient_color *= attenuation;
	final_color = vec4(ambient_color*light_color*light_power , texture(tex_sampler[pc.samplerIndex], uv).a);

}
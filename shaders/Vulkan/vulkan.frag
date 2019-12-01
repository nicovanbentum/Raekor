#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_vulkan_glsl : enable

// in vars
layout(location = 0) in vec4 color;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 frag_pos;
layout(location = 4) in vec3 lightPos;

// in uniforms
layout(set = 0, binding = 1) uniform sampler2D tex_sampler[24]; 
layout(push_constant) uniform pushConstants {
	int samplerIndex;
} pc;

// out vars
layout(location = 0) out vec4 final_color;



void main() {
	vec3 light_color = vec3(1.0, 1.0, 1.0);
	float ambient_strength = 0.5;
	vec3 ambient = ambient_strength * light_color * vec3(texture(tex_sampler[pc.samplerIndex], uv));


	vec3 norm = normalize(normal);
	vec3 lightdir = normalize(lightPos - frag_pos);
	float diff = max(dot(norm, lightdir), 0.0);
	vec3 diffuse = diff * light_color * vec3(texture(tex_sampler[pc.samplerIndex], uv));

    vec4 objectColor = texture(tex_sampler[pc.samplerIndex], uv);
	vec3 fragColor = (ambient * diffuse) * objectColor.xyz;
	final_color = vec4(fragColor, objectColor.a);
}
#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_vulkan_glsl : enable

// in vars
layout(location = 0) in vec4 color;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 pos;
layout(location = 4) in vec3 light_pos;

// in uniforms
layout(set = 0, binding = 1) uniform sampler2D tex_sampler[24]; 
layout(push_constant) uniform pushConstants {
    int samplerIndex;
} pc;

// out vars
layout(location = 0) out vec4 final_color;

void main() {
    // LIGHT CONSTANTS
    float constant = 1.0f;
    float linear = 0.7;
    float quadratic = 1.8;
    vec3 light_color = vec3(1.0, 0.7725, 0.56);

    vec4 sampled = texture( tex_sampler[pc.samplerIndex], uv);

    // ambient
    float ambient_strength = 0.1f;
    vec3 ambient = ambient_strength * sampled.xyz;

    // diffuse
    vec3 norm = normalize(normal);
    vec3 view_dir = normalize(-pos);

    vec3 light_dir = normalize(light_pos - pos);
    float diff = clamp(dot(norm, light_dir), 0, 1);
    vec3 diffuse = light_color * diff * sampled.rgb;

    // specular
    vec3 halfway_dir = normalize(light_dir + view_dir);
    vec3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(halfway_dir, norm), 0.0), 32.0f);
    vec3 specular = vec3(1.0, 1.0, 1.0) * spec * sampled.rgb;

    // distance between the light and the vertex
    float distance = length(light_pos - pos);
    float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));

    ambient = ambient * attenuation;
    diffuse = diffuse * attenuation;
    specular = specular * attenuation;

    vec3 result = ambient + diffuse + specular;
    final_color = vec4(result, sampled.a);
}
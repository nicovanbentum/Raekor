#version 330 core

// in vars
in vec4 color;
in vec2 uv;
in vec3 normal;
in vec3 pos;
in vec3 light_pos;
in vec3 light_angle;

// output data back to our openGL program
out vec4 final_color;

// constant mesh values
uniform sampler2D sampler;

struct PointLight {
	vec3 position;
	vec3 color;
	float constant, linear, quad;
};

struct DirectionalLight {
	vec3 direction;
	vec3 color;
};

vec3 doLight(PointLight light);
vec3 doLight(DirectionalLight light);

void main()
{
	PointLight light;
	light.position = light_pos;
	light.color = vec3(1.0, 0.7725, 0.56);
    light.constant = 1.0f;
    light.linear = 0.7;
    light.quad = 1.8;

	DirectionalLight dirlight;
	dirlight.direction = light_angle;
	dirlight.color = vec3(1.0, 0.7725, 0.56);

    vec4 sampled = texture(sampler, uv);

    vec3 result = doLight(dirlight) * 0.1;
	result += doLight(light);
    final_color = vec4(result, sampled.a);
}

vec3 doLight(PointLight light) {
	vec4 sampled = texture(sampler, uv);

    // ambient
    float ambient_strength = 0.1f;
    vec3 ambient = ambient_strength * sampled.xyz;

    // diffuse
    vec3 norm = normalize(normal);
    vec3 view_dir = normalize(-pos);

    vec3 light_dir = normalize(light_pos - pos);
    float diff = clamp(dot(norm, light_dir), 0, 1);
    vec3 diffuse = light.color * diff * sampled.rgb;

    // specular
    vec3 halfway_dir = normalize(light_dir + view_dir);
    vec3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(halfway_dir, norm), 0.0), 32.0f);
    vec3 specular = vec3(1.0, 1.0, 1.0) * spec * sampled.rgb;

    // distance between the light and the vertex
    float distance = length(light_pos - pos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quad * (distance * distance));

    ambient = ambient * attenuation;
    diffuse = diffuse * attenuation;
    specular = specular * attenuation;

    return (ambient + diffuse + specular);
}

vec3 doLight(DirectionalLight light) {
	vec4 sampled = texture(sampler, uv);

    // ambient
    float ambient_strength = 0.1f;
    vec3 ambient = ambient_strength * sampled.xyz;

    // diffuse
    vec3 norm = normalize(normal);
    vec3 view_dir = normalize(-pos);

    vec3 light_dir = normalize(light.direction);
    float diff = clamp(dot(norm, light_dir), 0, 1);
    vec3 diffuse = light.color * diff * sampled.rgb;

    // specular
    vec3 halfway_dir = normalize(light_dir + view_dir);
    vec3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(halfway_dir, norm), 0.0), 32.0f);
    vec3 specular = vec3(1.0, 1.0, 1.0) * spec * sampled.rgb;

    return (ambient + diffuse + specular);
}
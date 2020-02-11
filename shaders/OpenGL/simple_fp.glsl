#version 330 core

// in vars
in vec3 fragPos;
in vec4 color;
in vec2 uv;
in vec3 normal;
in vec3 pos;
in vec3 light_pos;
in vec3 light_angle;
in vec4 pos_light_space;

// output data back to our openGL program
out vec4 final_color;

// constant mesh values
uniform sampler2D meshTexture;
uniform sampler2D shadowMap;

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
float ShadowCalculation(vec4 fragPosLightSpace);

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

    vec4 sampled = texture(meshTexture, uv);

	float shadows = ShadowCalculation(pos_light_space);
    vec3 result = doLight(dirlight) * (1.0 - shadows);
	result += doLight(light);

    final_color = vec4(result, sampled.a);
}

float ShadowCalculation(vec4 fragPosLightSpace)
{
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadowMap, projCoords.xy).r; 
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // calculate bias (based on depth map resolution and slope)
    vec3 normal = normalize(normal);
    vec3 lightDir = normalize(vec3(-2.0f, 4.0f, -1.0f) - fragPos);
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    // check whether current frag pos is in shadow
    // float shadow = currentDepth - bias > closestDepth  ? 1.0 : 0.0;
    // PCF
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    
    // keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
    if(projCoords.z > 1.0)
        shadow = 0.0;
        
    return shadow;
}

vec3 doLight(PointLight light) {
	vec4 sampled = texture(meshTexture, uv);

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
	vec4 sampled = texture(meshTexture, uv);

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
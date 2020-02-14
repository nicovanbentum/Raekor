#version 440 core

layout (std140) uniform stuff {
    mat4 model, view, projection;
	mat4 lightSpaceMatrix;
	vec4 cameraPosition;
    vec4 DirLightPos;
	vec4 pointLightPos;
} ubo;

layout (std140, binding = 0) buffer extra {
	vec4 sunColor;
	float minBias, maxBias;
	float farPlane;
};

// in vars
in vec3 pos;
in vec3 fragPos;
in vec2 uv;
in vec3 normal;
in vec4 FragPosLightSpace;

in vec3 directionalLightPosition;
in vec3 directionalLightPositionViewSpace;
in vec3 pointLightPosition;
in vec3 pointLightPositionViewSpace;
in vec3 cameraPos;

// output data back to our openGL program
out vec4 final_color;

// constant mesh values
uniform sampler2D meshTexture;
uniform sampler2D shadowMap;
uniform samplerCube shadowMapOmni;

struct DirectionalLight {
	vec3 direction;
	vec3 position;
	vec3 color;
};

struct PointLight {
	vec3 position;
	vec3 color;
	float constant, linear, quad;
};

float ShadowCalculation(vec4 fragPosLightSpace);
float getShadow(PointLight light);
vec3 doLight(PointLight light);
vec3 doLight(DirectionalLight light);

void main()
{
	DirectionalLight dirLight;
	dirLight.color = sunColor.rgb;
	dirLight.position = directionalLightPositionViewSpace;

	PointLight light;
	light.position = pointLightPosition;
	light.color = vec3(1.0, 1.0, 1.0);
    light.constant = 0.0f;
    light.linear = 0.7;
    light.quad = 1.8;

	vec4 sampled = texture(meshTexture, uv);

    vec3 result = doLight(light);
	vec3 anotherresult = doLight(dirLight);
	//result += doLight(light);

    //final_color = vec4(result, sampled.a);
}

float ShadowCalculation(vec4 fragPosLightSpace) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    float closestDepth = texture(shadowMap, projCoords.xy).r; 
    float currentDepth = projCoords.z;

    vec3 normal = normalize(normal);
    vec3 lightDir = normalize(directionalLightPositionViewSpace - pos);
    float bias = max(maxBias * (1.0 - dot(normal, lightDir)), minBias);
    
	// simplest PCF algorithm
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    
    // keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
    if(projCoords.z > 1.0) shadow = 0.0;
        
    return shadow;
}

float getShadow(PointLight light) {
	vec3 frag2light = fragPos - light.position;
	float closestDepth = texture(shadowMapOmni, frag2light).r;


	// todo make the far plane an input variable
	closestDepth *= farPlane;
	final_color = vec4(vec3(closestDepth / farPlane), 1.0);

	float currentDepth = length(frag2light);

	float bias = 0.05;
	float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;


	return shadow;
}


vec3 doLight(PointLight light) {
	vec4 sampled = texture(meshTexture, uv);

    // ambient
    float ambient_strength = 1.0f;
    vec3 ambient = ambient_strength * sampled.xyz;

    // diffuse
    vec3 norm = normalize(normal);
    vec3 view_dir = normalize(-pos);

    vec3 light_dir = normalize(pointLightPositionViewSpace - pos);
    float diff = clamp(dot(norm, light_dir), 0, 1);
    vec3 diffuse = light.color * diff * sampled.rgb;

    // specular
    vec3 halfway_dir = normalize(light_dir + view_dir);
    vec3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(halfway_dir, norm), 0.0), 32.0f);
    vec3 specular = vec3(1.0, 1.0, 1.0) * spec * sampled.rgb;

    // distance between the light and the vertex
    float distance = length(pointLightPositionViewSpace - pos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quad * (distance * distance));

    ambient = ambient * attenuation;
    diffuse = diffuse * attenuation;
    specular = specular * attenuation;

	float shadowAmount = getShadow(light);
	diffuse *= shadowAmount;
	specular *= shadowAmount;

    return (ambient + diffuse + specular);
}

vec3 doLight(DirectionalLight light) {
	vec4 sampled = texture(meshTexture, uv);
    // vec4 sampled = vec4(0.0, 1.0, 0.0, 1.0);
    vec3 normal = normalize(normal);
    // ambient
    vec3 ambient = 0.1 * sampled.xyz;
	
	// diffuse
    vec3 norm = normalize(normal);
    vec3 view_dir = normalize(-pos);

    vec3 light_dir = normalize(light.position - pos);
    float diff = clamp(dot(norm, light_dir), 0, 1);
    vec3 diffuse = light.color * diff * sampled.rgb;

    // specular
    vec3 halfway_dir = normalize(light_dir + view_dir);
    vec3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(halfway_dir, norm), 0.0), 32.0f);
    vec3 specular = vec3(1.0, 1.0, 1.0) * spec * sampled.rgb;

	float shadowAmount = 1.0 - ShadowCalculation(FragPosLightSpace);
	diffuse *= shadowAmount;
	specular *= shadowAmount;

    return (ambient + diffuse + specular);
}
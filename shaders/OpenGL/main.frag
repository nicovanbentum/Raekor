#version 440 core

const float PI = 3.14159265359;

#define MAX_POINT_LIGHTS 10
#define MAX_DIR_LIGHTS 1

struct DirectionalLight {
	vec4 position;
	vec4 color;
};

struct PointLight {
	vec4 position;
	vec4 color;
};

layout (std140) uniform stuff {
	mat4 view, projection;
	mat4 lightSpaceMatrix;
	vec4 cameraPosition;
    DirectionalLight dirLights[MAX_DIR_LIGHTS];
    PointLight pointLights[MAX_POINT_LIGHTS];
} ubo;

uniform vec4 sunColor;
uniform float minBias;
uniform float maxBias;
uniform float farPlane;
uniform vec3 bloomThreshold;

uniform int pointLightCount;
uniform int directionalLightCount;

// in vars
in vec2 uv;

// output data back to our openGL program
layout (location = 0) out vec4 finalColor;
layout (location = 1) out vec4 bloomColor;

// shadow maps
layout(binding = 0) uniform sampler2D shadowMap;
layout(binding = 1) uniform samplerCube shadowMapOmni;

// GBUFFER textures
layout(binding = 2) uniform sampler2D gPositions;
layout(binding = 3) uniform sampler2D gColors;
layout(binding = 4) uniform sampler2D gNormals;
layout(binding = 5) uniform sampler2D SSAO;

vec3 doLight(PointLight light);
vec3 doLight(DirectionalLight light);
float getShadow(PointLight light);
float getShadow(DirectionalLight light);

// vars retrieved from the Gbuffer TODO: make them not global?
vec3 position;
vec3 normal;
vec4 sampled;
float AO = 1.0;

void main()
{
	sampled = texture(gColors, uv);
	normal = texture(gNormals, uv).xyz;
	position = texture(gPositions, uv).xyz;

    AO = texture(SSAO, uv).x;
    AO = clamp(AO, 0.0, 1.0);

    vec3 result = vec3(0.0, 0.0, 0.0);

    // caculate point lights contribution
    for(uint i = 0; i < pointLightCount; i++) {
        result += doLight(ubo.pointLights[i]);
    }

    // calculate directional lights contribution
    for(uint i = 0; i < directionalLightCount; i++) {
        result += doLight(ubo.dirLights[i]);
    }

    finalColor = vec4(result, sampled.a);

	float brightness = dot(finalColor.rgb, bloomThreshold);
	if(brightness > 1.0) 
		bloomColor = vec4(finalColor.rgb, 1.0);
	else 
		bloomColor = vec4(0.0, 0.0, 0.0, 1.0);
}

float getShadow(DirectionalLight light) {
	vec4 FragPosLightSpace = ubo.lightSpaceMatrix * vec4(position, 1.0);
    vec3 projCoords = FragPosLightSpace.xyz / FragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    float currentDepth = projCoords.z;

	vec3 direction = normalize(light.position.xyz - position);
    float bias = max(maxBias * (1.0 - dot(normal, direction)), minBias);
    
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
    
    // keep the shadow at 0.0 when outside the far plane region of the light's frustum
    if(projCoords.z > 1.0) shadow = 0.0;
        
    return shadow;
}

// array of offset direction for sampling
vec3 gridSamplingDisk[20] = vec3[]
(
   vec3(1, 1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1, 1,  1), 
   vec3(1, 1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
   vec3(1, 1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1, 1,  0),
   vec3(1, 0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1, 0, -1),
   vec3(0, 1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0, 1, -1)
);

float getShadow(PointLight light) {
	vec3 frag2light = position - light.position.xyz;
	float currentDepth = length(frag2light);

	float closestDepth = texture(shadowMapOmni, frag2light).r;
	closestDepth *= farPlane;
	
	// PCF sampling
	float shadow = 0.0;
    float bias = 0.05;
    int samples = 20;

	return currentDepth - bias > closestDepth ? 1.0 : 0.0;

    float viewDistance = length(ubo.cameraPosition.xyz - position);
    float diskRadius = (1.0 + (viewDistance / farPlane)) / farPlane;
    for(int i = 0; i < samples; ++i) {
        float closestDepth = texture(shadowMapOmni, frag2light + gridSamplingDisk[i] * diskRadius).r;
        closestDepth *= farPlane;   // undo mapping [0;1]
        if(currentDepth - bias > closestDepth)
            shadow += 1.0;
    }
    shadow /= float(samples);

	return shadow;
}


vec3 doLight(PointLight light) {
    // hardcoded for now
    float constant = 1.0;
    float linear = 0.7;
    float quad = 1.8;

    // ambient
    vec3 ambient = 0.05 * sampled.xyz * AO;

	vec3 direction = normalize(light.position.xyz - position);
	vec3 cameraDirection = normalize(ubo.cameraPosition.xyz - position);
    
	float diff = clamp(dot(normal, direction), 0, 1);
    vec3 diffuse = light.color.xyz * diff * sampled.rgb;

    // specular
    vec3 halfwayDir = normalize(direction + cameraDirection);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0f);
    vec3 specular = vec3(1.0, 1.0, 1.0) * spec * sampled.rgb;

    // distance between the light and the vertex
    float distance = length(light.position.xyz - position);
    float attenuation = 1.0 / (constant + linear * distance + quad * (distance * distance));

    ambient = ambient * attenuation;
    diffuse = diffuse * attenuation;
    specular = specular * attenuation;

     float shadowAmount = 1.0 - getShadow(light);
     diffuse *= shadowAmount;
     specular *= shadowAmount;

    return (ambient + diffuse + specular);
}

vec3 doLight(DirectionalLight light) {
	// ambient
    vec3 ambient = 0.05 * sampled.xyz * AO;

	vec3 direction = normalize(light.position.xyz - position);
	vec3 cameraDirection = normalize(ubo.cameraPosition.xyz - position);
    
	float diff = clamp(dot(normal, direction), 0, 1);
    vec3 diffuse = light.color.xyz * diff * sampled.rgb;

    // specular
    vec3 halfwayDir = normalize(direction + cameraDirection);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0f);
    vec3 specular = vec3(1.0, 1.0, 1.0) * spec * sampled.rgb;

	float shadowAmount = 1.0 - getShadow(light);
	diffuse *= shadowAmount;
	specular *= shadowAmount;

    return (ambient + diffuse + specular);
}
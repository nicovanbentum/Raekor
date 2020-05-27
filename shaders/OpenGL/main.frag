#version 440 core

const float PI = 3.14159265359;

const float voxelGridSize = 150.0f;

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

layout(binding = 6) uniform sampler3D voxels;

const float MAX_DIST = 100.0;
const float ALPHA_THRESH = 0.95;

// 6 60 degree cone
const int NUM_CONES = 6;
vec3 coneDirections[6] = vec3[]
(                            vec3(0, 1, 0),
                            vec3(0, 0.5, 0.866025),
                            vec3(0.823639, 0.5, 0.267617),
                            vec3(0.509037, 0.5, -0.700629),
                            vec3(-0.509037, 0.5, -0.700629),
                            vec3(-0.823639, 0.5, 0.267617)
                            );
float coneWeights[6] = float[](0.25, 0.15, 0.15, 0.15, 0.15, 0.15);

vec3 doLight(PointLight light);
vec3 doLight(DirectionalLight light);
float getShadow(PointLight light);
float getShadow(DirectionalLight light);

// vars retrieved from the Gbuffer TODO: make them not global?
vec3 position;
vec3 normal;
vec4 sampled;
float AO = 1.0;

vec4 sampleVoxels(vec3 worldPosition, float lod) {
    vec3 offset = vec3(1.0 / 128, 1.0 / 128, 0); // Why??
    vec3 voxelTextureUV = position / (voxelGridSize * 0.5);
    voxelTextureUV = voxelTextureUV * 0.5 + 0.5 + offset;
    return textureLod(voxels, voxelTextureUV, lod);
}

// Third argument to say how long between steps?
vec4 coneTrace(vec3 direction, float tanHalfAngle, out float occlusion) {
    // lod level 0 mipmap is full size, level 1 is half that size and so on
    float lod = 0.0;
    vec3 color = vec3(0);
    float alpha = 0.0;
    occlusion = 0.0;

    float voxelWorldSize = voxelGridSize / 128;
    float dist = voxelWorldSize; // Start one voxel away to avoid self occlusion
    vec3 startPos = position + normal * voxelWorldSize; // Plus move away slightly in the normal direction to avoid
                                                                    // self occlusion in flat surfaces
    while(dist < MAX_DIST && alpha < ALPHA_THRESH) {
        // smallest sample diameter possible is the voxel size
        float diameter = max(voxelWorldSize, 2.0 * tanHalfAngle * dist);
        float lodLevel = log2(diameter / voxelWorldSize);
        vec4 voxelColor = sampleVoxels(startPos + dist * direction, lodLevel);

        // front-to-back compositing
        float a = (1.0 - alpha);
        color += a * voxelColor.rgb;
        alpha += a * voxelColor.a;
        //occlusion += a * voxelColor.a;
        occlusion += (a * voxelColor.a) / (1.0 + 0.03 * diameter);
        dist += diameter * 0.5; // smoother
        //dist += diameter; // faster but misses more voxels
    }

    return vec4(color, alpha);
}

vec4 indirectLight(out float occlusion_out) {
    vec4 color = vec4(0);
    occlusion_out = 0.0;

    for(int i = 0; i < NUM_CONES; i++) {
        float occlusion = 0.0;
        // 60 degree cones -> tan(30) = 0.577
        // 90 degree cones -> tan(45) = 1.0
        color += coneWeights[i] * coneTrace(coneDirections[i], 0.577, occlusion);
        occlusion_out += coneWeights[i] * occlusion;
    }

    occlusion_out = 1.0 - occlusion_out;

    return color;
}

void main()
{
	sampled = texture(gColors, uv);
	normal = texture(gNormals, uv).xyz;
	position = texture(gPositions, uv).xyz;

    //  AO = texture(SSAO, uv).x;
    //  AO = clamp(AO, 0.0, 1.0);

    // calculate directional lights contribution
    for(uint i = 0; i < directionalLightCount; i++) {
        //result += doLight(ubo.dirLights[i]);
    }

    vec3 diffuse = doLight(ubo.dirLights[0]);

    // Indirect diffuse light
    float occlusion = 0.0;
    vec3 indirectDiffuseLight = indirectLight(occlusion).rgb;
    indirectDiffuseLight = 4.0 * indirectDiffuseLight;

    // Sum direct and indirect diffuse light and tweak a little bit
    occlusion = min(1.0, 1.5 * occlusion); // Make occlusion brighter
    vec3 diffuseReflection = 2.0 * occlusion * (diffuse + indirectDiffuseLight) * sampled.rgb;

    finalColor = vec4(diffuseReflection, sampled.a);

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

    //  float shadowAmount = 1.0 - getShadow(light);
    //  diffuse *= shadowAmount;
    //  specular *= shadowAmount;

    return (ambient + diffuse + specular);
}

vec3 doLight(DirectionalLight light) {
	// ambient
    vec3 ambient = 0.0 * sampled.xyz * AO;

	vec3 direction = normalize(light.position.xyz - position);
	vec3 cameraDirection = normalize(ubo.cameraPosition.xyz - position);
    
	float diff = clamp(dot(normal, direction), 0, 1);
    vec3 diffuse = light.color.xyz * diff * sampled.rgb;

    // specular
    vec3 halfwayDir = normalize(direction + cameraDirection);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0f);
    vec3 specular = vec3(1.0, 1.0, 1.0) * spec * sampled.rgb;

	// float shadowAmount = 1.0 - getShadow(light);
	// diffuse *= shadowAmount;
	// specular *= shadowAmount;

    return (ambient + diffuse + specular);
}
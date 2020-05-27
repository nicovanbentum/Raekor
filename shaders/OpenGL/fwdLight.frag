#version 440 core

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

in vec2 uv;
in vec3 position;
in vec3 normal;
in vec3 tangent;
in vec3 bitangent;
in vec3 cameraDirection;

out vec4 color;

// constant mesh values
layout(binding = 0) uniform sampler3D voxels;
layout(binding = 1) uniform sampler2D meshTexture;
layout(binding = 2) uniform sampler2D normalMap;
layout(binding = 3) uniform sampler2D shadowMap;


// Voxel stuff
const float VoxelGridWorldSize = 150.0;
const int VoxelDimensions = 128;

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

mat3 tangentToWorld;

vec4 sampleVoxels(vec3 worldPosition, float lod) {
    vec3 offset = vec3(1.0 / VoxelDimensions, 1.0 / VoxelDimensions, 0); // Why??
    vec3 voxelTextureUV = worldPosition / (VoxelGridWorldSize * 0.5);
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

    float voxelWorldSize = VoxelGridWorldSize / VoxelDimensions;
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
        occlusion += (a * voxelColor.a) / (1.0 + 0.03 * diameter);
        dist += diameter * 0.5; // smoother
    }

    return vec4(color, alpha);
}

vec4 indirectLight(out float occlusion_out) {
    vec4 color = vec4(0);
    occlusion_out = 0.0;

    for(int i = 0; i < NUM_CONES; i++) {
        float occlusion = 0.0;
        color += coneWeights[i] * coneTrace(tangentToWorld * coneDirections[i], 0.577, occlusion);
        occlusion_out += coneWeights[i] * occlusion;
    }

    occlusion_out = 1.0 - occlusion_out;

    return color;
}

vec3 calcBumpNormal() {
    return normalize(tangentToWorld * texture(normalMap, uv).xyz);
}

float getShadow(DirectionalLight light) {
	vec4 FragPosLightSpace = ubo.lightSpaceMatrix * vec4(position, 1.0);
    vec3 projCoords = FragPosLightSpace.xyz / FragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    float currentDepth = projCoords.z;

	vec3 direction = normalize(light.position.xyz - position);
    float bias = max(0.0 * (1.0 - dot(normal, direction)), 0.002);
    
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

void main() {
    vec4 materialColor = texture(meshTexture, uv);
    float alpha = materialColor.a;

    if(alpha < 0.5) {
        discard;
    }
    
    tangentToWorld = inverse(transpose(mat3(tangent, normal, bitangent)));

    DirectionalLight light = ubo.dirLights[0];

    float shadow = 1.0 - getShadow(light);

	vec3 direction = normalize(light.position.xyz - position);
    float diff = clamp(dot(normal, direction), 0, 1);
    vec3 diffuse = light.color.xyz * diff * materialColor.rgb;

    // Calculate diffuse light
    vec3 diffuseReflection;

    // Indirect diffuse light
    float occlusion = 0.0;
    vec3 indirectDiffuseLight = indirectLight(occlusion).rgb;
    indirectDiffuseLight = 4.0 * indirectDiffuseLight;

    // Sum direct and indirect diffuse light and tweak a little bit
    occlusion = min(1.0, 1.5 * occlusion); // Make occlusion brighter
    diffuseReflection = 2.0 * occlusion * (diffuse + indirectDiffuseLight) * materialColor.rgb;
    
    color = vec4(diffuseReflection, alpha);
}
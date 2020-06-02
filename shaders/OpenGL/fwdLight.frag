#version 440 core

#define MAX_POINT_LIGHTS 10
#define MAX_DIR_LIGHTS 1

struct DirectionalLight {
    vec3 direction;
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
in mat3 TBN;
in vec3 cameraDirection;
in vec4 depthPosition;

out vec4 color;

// constant mesh values
layout(binding = 0) uniform sampler3D voxels;
layout(binding = 1) uniform sampler2D meshTexture;
layout(binding = 2) uniform sampler2D normalMap;
layout(binding = 3) uniform sampler2DShadow shadowMap;


// Voxel stuff
const float VoxelGridWorldSize = 150.0;
int VoxelDimensions;

// source: http://simonstechblog.blogspot.com/2013/01/implementing-voxel-cone-tracing.html
// 6 60 degree cone
const int NUM_CONES = 6;
vec3 coneDirections[6] = vec3[] (
    vec3(0, 1, 0),
    vec3(0, 0.5, 0.866025),
    vec3(0.823639, 0.5, 0.267617),
    vec3(0.509037, 0.5, -0.700629),
    vec3(-0.509037, 0.5, -0.700629),
    vec3(-0.823639, 0.5, 0.267617)
);
float coneWeights[6] = float[](0.25, 0.15, 0.15, 0.15, 0.15, 0.15);

// cone tracing through ray marching
// a ray is just a starting vector and a direction
// so it goes : sample, move in direction, sample, move in direction, sample etc
// until we hit opacity 1 which means we hit a solid object
vec4 coneTrace(in vec3 p, in vec3 n, in vec3 direction, in float coneAperture, out float occlusion) {
    vec4 colour = vec4(0);
    occlusion = 0.0;

    float voxelWorldSize = VoxelGridWorldSize / VoxelDimensions;
     // start one voxel away from the current vertex' position
    float dist = voxelWorldSize; 
    vec3 startPos = p + n * voxelWorldSize; 
    
    while(dist < VoxelGridWorldSize && colour.a < 1) {
        float diameter = max(voxelWorldSize, 2 * coneAperture * dist);
        float mip = log2(diameter / voxelWorldSize);

        vec3 offset = vec3(1.0 / VoxelDimensions, 1.0 / VoxelDimensions, 0);
        vec3 voxelTextureUV = (startPos + dist * direction) / (VoxelGridWorldSize * 0.5);
        voxelTextureUV = voxelTextureUV * 0.5 + 0.5 + offset;
        vec4 voxel_colour = textureLod(voxels, voxelTextureUV, mip);

        float a = (1.0 - colour.a);
        colour.rgb += a * voxel_colour.rgb;
        colour.a += a * voxel_colour.a;
        occlusion += (a * voxel_colour.a) / (1.0 + 0.03 * diameter);

        // move along the ray
        dist += diameter * 0.5;
    }

    return colour;
}

vec4 coneTraceBounceLight(in vec3 p, in vec3 n, out float occlusion_out) {
    vec4 color = vec4(0);
    occlusion_out = 0.0;

    for(int i = 0; i < NUM_CONES; i++) {
        float occlusion = 0.0;
        color += coneWeights[i] * coneTrace(p, n, coneDirections[i], tan(radians(30)), occlusion);
        occlusion_out += coneWeights[i] * occlusion;
    }

    occlusion_out = 1.0 - occlusion_out;

    return color;
}

void main() {
    VoxelDimensions = textureSize(voxels, 0).x;
    vec4 albedo = texture(meshTexture, uv);

    if(albedo.a < 0.5) {
        discard;
    }

	vec4 normalColor = texture(normalMap, uv);
	normalColor = normalize(normalColor * 2.0 - 1.0);
	normalColor = vec4(normalize(TBN * normalColor.xyz), 1.0);

    float shadowAmount = texture(shadowMap, vec3(depthPosition.xy, (depthPosition.z - 0.0005)/depthPosition.w));

    DirectionalLight light = ubo.dirLights[0];

	vec3 direction = normalize(-light.direction);
    float diff = clamp(dot(normalColor.xyz, direction) * shadowAmount, 0, 1);
    vec3 directLight = light.color.xyz * diff * albedo.rgb;

    // specular
    
    vec3 cameraDirection = normalize(ubo.cameraPosition.xyz - position);
    vec3 reflectDir = reflect(-cameraDirection, normalColor.xyz);


    float specOcclusion = 0.0;
    vec4 specularTraced = coneTrace(position, normalColor.xyz, reflectDir, 0.07, specOcclusion);
    vec3 specular = vec3(1.0, 1.0, 1.0) * specularTraced.xyz * light.color.xyz;
    

    float occlusion = 0.0;
    vec3 bounceLight = coneTraceBounceLight(position, normalColor.xyz, occlusion).rgb;

    vec3 diffuseReflection = occlusion * (directLight + bounceLight + specular) * albedo.rgb;

    color = vec4(diffuseReflection,  albedo.a);
}
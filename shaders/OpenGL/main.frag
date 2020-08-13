#version 440 core

const float PI = 3.14159265359;

// light definitions
#define MAX_POINT_LIGHTS 10
#define MAX_DIR_LIGHTS 1

struct DirectionalLight {
	vec4 direction;
	vec4 color;
};

struct PointLight {
	vec4 position;
	vec4 color;
};

// cpu side uniform buffer
layout (std140) uniform stuff {
	mat4 view, projection;
	mat4 lightSpaceMatrix;
	vec4 cameraPosition;
    DirectionalLight dirLights[MAX_DIR_LIGHTS];
    PointLight pointLights[MAX_POINT_LIGHTS];
} ubo;

// TODO: some of these are no longer in use since VCT
uniform vec4 sunColor;
uniform float minBias;
uniform float maxBias;
uniform float farPlane;
uniform vec3 bloomThreshold;

uniform int pointLightCount;
uniform int directionalLightCount;

uniform float voxelsWorldSize;

// in vars
in vec2 uv;

// output data back to our openGL program
layout (location = 0) out vec4 finalColor;
layout (location = 1) out vec4 bloomColor;

// shadow maps
layout(binding = 0) uniform sampler2DShadow shadowMap;
layout(binding = 1) uniform samplerCube shadowMapOmni;

// GBUFFER textures
layout(binding = 2) uniform sampler2D gPositions;
layout(binding = 3) uniform sampler2D gColors;
layout(binding = 4) uniform sampler2D gNormals;
layout(binding = 7) uniform sampler2D gMetallicRoughness;

// previous render pass attachments
layout(binding = 5) uniform sampler2D SSAO;
layout(binding = 6) uniform sampler3D voxels;

// vars retrieved from the Gbuffer TODO: make them not global?
vec3 position;
vec3 normal;
vec3 tangent;

// Voxel stuff
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

// source: https://wickedengine.net/2017/08/30/voxel-based-global-illumination/
const vec3 CONES[] = 
{
	vec3(0.57735, 0.57735, 0.57735),
	vec3(0.57735, -0.57735, -0.57735),
	vec3(-0.57735, 0.57735, -0.57735),
	vec3(-0.57735, -0.57735, 0.57735),
	vec3(-0.903007, -0.182696, -0.388844),
	vec3(-0.903007, 0.182696, 0.388844),
	vec3(0.903007, -0.182696, 0.388844),
	vec3(0.903007, 0.182696, -0.388844),
	vec3(-0.388844, -0.903007, -0.182696),
	vec3(0.388844, -0.903007, 0.182696),
	vec3(0.388844, 0.903007, -0.182696),
	vec3(-0.388844, 0.903007, 0.182696),
	vec3(-0.182696, -0.388844, -0.903007),
	vec3(0.182696, 0.388844, -0.903007),
	vec3(-0.182696, 0.388844, 0.903007),
	vec3(0.182696, -0.388844, 0.903007)
};

// cone tracing through ray marching
// a ray is just a starting vector and a direction
// so it goes : sample, move in direction, sample, move in direction, sample etc
// until we hit opacity 1 which means we hit a solid object
vec4 coneTrace(in vec3 p, in vec3 n, in vec3 direction, in float coneAperture, out float occlusion) {
    vec4 colour = vec4(0);
    occlusion = 0.0;

    float voxelSize = voxelsWorldSize / VoxelDimensions;
     // start one voxel away from the current vertex' position
    float dist = voxelSize; 
    vec3 startPos = p + n * voxelSize * 2 * sqrt(2); 
    
    while(dist <= voxelsWorldSize && colour.a < 1.0) {
        float diameter = max(voxelSize, 2 * coneAperture * dist);
        float mip = log2(diameter / voxelSize);

        // create vec3 for reading voxel texture from world vector
        vec3 offset = vec3(1.0 / VoxelDimensions, 1.0 / VoxelDimensions, 0);
        vec3 voxelTextureUV = (startPos + dist * direction) / (voxelsWorldSize * 0.5);
        voxelTextureUV = voxelTextureUV * 0.5 + 0.5 + offset;
        vec4 voxel_colour = textureLod(voxels, voxelTextureUV, mip);

        // back-to-front alpha 
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
        vec3 coneDirection = normalize(CONES[i] + n);
        coneDirection *= dot(coneDirection, n) < 0 ? -1 : 1;

        float occlusion = 0.0;
        // aperture is half tan of angle
        color += coneTrace(p, n, coneDirection, tan(PI * 0.5f * 0.33f), occlusion);
        occlusion_out += occlusion;
    }

    color /= NUM_CONES;
    color.a = clamp(color.a, 0, 1);
    occlusion_out = 1.0 - occlusion_out;

    return max(vec4(0), color);
}

float getShadow(DirectionalLight light, vec3 position) {
    vec4 FragPosLightSpace = ubo.lightSpaceMatrix * vec4(position, 1.0);
    FragPosLightSpace.xyz = FragPosLightSpace.xyz * 0.5 + 0.5;

    float currentDepth = FragPosLightSpace.z;

	vec3 direction = normalize(-light.direction.xyz);
    float bias = max(maxBias * (1.0 - dot(normal, direction)), minBias);
    
	// simplest PCF algorithm
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -3; x <= 3; ++x) {
        for(int y = -3; y <= 3; ++y) {
            float pcfDepth = texture(shadowMap, vec3(FragPosLightSpace.xy + vec2(x, y) * texelSize, (FragPosLightSpace.z - 0.0005)/FragPosLightSpace.w)).r; 
            shadow += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;    
        }    
    }
    shadow /= 49.0;
    
    // keep the shadow at 0.0 when outside the far plane region of the light's frustum
    if(FragPosLightSpace.z > 1.0) shadow = 0.0;
        
    return shadow;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

void main()
{
    VoxelDimensions = textureSize(voxels, 0).x;
	vec4 albedo = texture(gColors, uv);

    vec4 metallicRoughness = texture(gMetallicRoughness, uv);\
    float metallic = metallicRoughness.r;
    float roughness = metallicRoughness.g;

	normal = texture(gNormals, uv).xyz;
	position = texture(gPositions, uv).xyz;

    vec4 depthPosition = ubo.lightSpaceMatrix * texture(gPositions, uv);
    // from -1 1 to 0 1 range for uv coordinate
    depthPosition.xyz = depthPosition.xyz * 0.5 + 0.5;

    DirectionalLight light = ubo.dirLights[0];
    float shadowAmount = texture(shadowMap, vec3(depthPosition.xy, (depthPosition.z - 0.0005)/depthPosition.w));
    shadowAmount = 1.0 - getShadow(light, position);

    // get direct light
    vec3 direction = normalize(-light.direction.xyz);
    float diff = clamp(dot(normal.xyz, direction) * shadowAmount, 0, 1);
    vec3 diffuse = light.color.xyz * diff;

    // get specular light
    vec3 V = normalize(ubo.cameraPosition.xyz - position);
    vec3 reflectDir = reflect(-V, normal.xyz);

    // PBR
    ////////////////////////////////
    vec3 L = -light.direction.xyz;
    vec3 halfway = normalize(V + L);
    vec3 radiance = light.color.rgb * shadowAmount;

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo.rgb, metallic);

    float NDF = DistributionGGX(normal, halfway, roughness);
    float G = GeometrySmith(normal, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(halfway, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, V), 0.0) * max(dot(normal, L), 0.0);
    vec3 specular = numerator / max(denominator, 0.001);


    float NdotL = max(dot(normal, L), 0.0);
    vec3 Lo = (kD * albedo.rgb / PI + specular) * radiance * NdotL;
    
    // get first bounce light
    float occlusion = 0.0;
    vec3 bounceLight = coneTraceBounceLight(position, normal.xyz, occlusion).rgb;

    vec3 ambient = bounceLight * albedo.rgb;
    vec3 color = ambient + Lo;

    // combine all
    vec3 diffuseReflection = (radiance + bounceLight + specular) * albedo.rgb;
    finalColor = vec4(color, albedo.a);

    // BLOOM SEPERATION
	float brightness = dot(finalColor.rgb, bloomThreshold);
	if(brightness > 1.0) 
		bloomColor = vec4(finalColor.rgb, 1.0);
	else 
		bloomColor = vec4(0.0, 0.0, 0.0, 1.0);
}
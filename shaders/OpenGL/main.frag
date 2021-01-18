#version 450 core

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

uniform mat4 invViewProjection;

// in vars
layout(location = 0) in vec2 uv;

// output data back to our openGL program
layout (location = 0) out vec4 finalColor;
layout (location = 1) out vec4 bloomColor;

// shadow maps
layout(binding = 0) uniform sampler2DShadow shadowMap;
layout(binding = 1) uniform samplerCube shadowMapOmni;

// GBUFFER textures
layout(binding = 3) uniform sampler2D gColors;
layout(binding = 4) uniform sampler2D gNormals;
layout(binding = 7) uniform sampler2D gMetallicRoughness;
layout(binding = 8) uniform sampler2D gDepth;

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
vec4 coneTrace(in vec3 p, in vec3 n, in vec3 coneDirection, in float coneAperture, out float occlusion) {
    vec4 colour = vec4(0);
    occlusion = 0.0;

    float voxelSize = voxelsWorldSize / VoxelDimensions;
     // start one voxel away from the current vertex' position
    float dist = voxelSize; 
    vec3 startPos = p + n * voxelSize; 
    
    while(dist <= voxelsWorldSize && colour.a < 1.0) {
        float diameter = max(voxelSize, 2 * coneAperture * dist);
        float mip = log2(diameter / voxelSize);

        // create vec3 for reading voxel texture from world vector
        vec3 offset = vec3(1.0 / VoxelDimensions, 1.0 / VoxelDimensions, 0);
        vec3 voxelTextureUV = (startPos + dist * coneDirection) / (voxelsWorldSize * 0.5);
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

vec4 coneTraceRadiance(in vec3 p, in vec3 n, out float occlusion_out) {
    vec4 color = vec4(0);
    occlusion_out = 0.0;

    for(int i = 0; i < NUM_CONES; i++) {
        vec3 coneDirection = normalize(CONES[i] + n);
        coneDirection *= dot(coneDirection, n) < 0 ? -1 : 1;

        float occlusion = 0.0;
        // aperture is half tan of angle
        color += coneTrace(p, n, coneDirection, tan(PI * 0.5f * 0.33f), occlusion);
        occlusion_out += coneWeights[i] * occlusion;
    }

    color /= NUM_CONES;
    color.a = clamp(color.a, 0, 1);
    occlusion_out = 1.0 - occlusion_out;

    return max(vec4(0), color);
}

vec4 coneTraceReflection(in vec3 P, in vec3 N, in vec3 V, in float roughness, out float occlusion) {
    float aperture = tan(roughness * PI * 0.5f * 0.1f);
    vec3 coneDirection = reflect(-V, N);

    vec4 reflection = coneTrace(P, N, coneDirection, aperture, occlusion);
    return vec4(max(vec3(0), reflection.rgb), clamp(reflection.a, 0, 1));
}

float InterleavedGradientNoise(vec2 pixel) {
  vec3 magic = vec3(0.06711056f, 0.00583715f, 52.9829189f);
  return fract(magic.z * fract(dot(pixel, magic.xy)));
}

float rand(vec2 n) { 
	return fract(sin(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

float noise(vec2 p){
	vec2 ip = floor(p);
	vec2 u = fract(p);
	u = u*u*(3.0-2.0*u);
	
	float res = mix(
		mix(rand(ip),rand(ip+vec2(1.0,0.0)),u.x),
		mix(rand(ip+vec2(0.0,1.0)),rand(ip+vec2(1.0,1.0)),u.x),u.y);
	return res*res;
}

// from https://www.gamedev.net/tutorials/programming/graphics/contact-hardening-soft-shadows-made-fast-r4906/
vec2 VogelDiskSample(int sampleIndex, int samplesCount, float phi) {
    float GoldenAngle = 2.4f;

    float r = sqrt(sampleIndex + 0.5f) / sqrt(samplesCount);
    float theta = sampleIndex * GoldenAngle + phi;

    float sine = sin(theta);
    float cosine = cos(theta);

    return vec2(r * cosine, r* sine);
}

float AvgBlockersDepthToPenumbra(float lightSize, float z_shadowMapView, float avgBlockersDepth) {
    return lightSize * (z_shadowMapView - avgBlockersDepth) / avgBlockersDepth;
}

vec2 VogelDiskScale(vec2 shadowMapSize, int samplingKernelSize) {
    vec2 texelSize = 2.0 / shadowMapSize;
    return texelSize * samplingKernelSize;
}

float Penumbra(float gradientNoise, vec4 FragPosLightSpace, int samplesCount) {
    float avgBlockersDepth = 0.0f;
    float blockersCount = 0.0f;

    const int KernelSize = 4;
    vec2 shadowMapSize = textureSize(shadowMap, 0).xy;
    vec2 penumbraFilterMaxSize = VogelDiskScale(shadowMapSize, KernelSize);

    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    for(int i = 0; i < samplesCount; i ++) {
        vec2 offsetUv = VogelDiskSample(i, samplesCount, gradientNoise) * penumbraFilterMaxSize;

        float sampleDepth = texture(shadowMap, vec3(FragPosLightSpace.xy, FragPosLightSpace.z)).r; 

        // if the sampled depth is smaller than the fragments depth, its a blocker
        if(sampleDepth < FragPosLightSpace.z)
        {
            avgBlockersDepth += sampleDepth;
            blockersCount += 1.0f;
        }
    }

    if(blockersCount > 0.0f) {
        avgBlockersDepth /= blockersCount;
        return AvgBlockersDepthToPenumbra(0.1, FragPosLightSpace.z, avgBlockersDepth);
    } else {
        return 0.0f;
    }
}

float getShadow(DirectionalLight light, vec3 position) {
    vec4 FragPosLightSpace = ubo.lightSpaceMatrix * vec4(position, 1.0);
    FragPosLightSpace.xyz /= FragPosLightSpace.w;
    FragPosLightSpace.xyz = FragPosLightSpace.xyz * 0.5 + 0.5;

    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    const int KernelSize = 4;
    vec2 penumbraFilterMaxSize = VogelDiskScale(textureSize(shadowMap, 0).xy, KernelSize);

    float gradientNoise = PI * 2.0 * InterleavedGradientNoise(gl_FragCoord.xy);
    int samplesCount = 16;

    float penumbra = Penumbra(gradientNoise, FragPosLightSpace, samplesCount);

    float shadow = 0.0;
    for(int i = 0; i < samplesCount; i++) {
        vec2 offsetUv = VogelDiskSample(i, samplesCount, gradientNoise);
        //offsetUv = offsetUv * penumbra;

        float sampled = texture(shadowMap, vec3(FragPosLightSpace.xy + offsetUv * texelSize, FragPosLightSpace.z)).r;
        shadow += 1.0 - sampled; 
    }

    shadow /= samplesCount;
    
    // keep the shadow at 0.0 when outside the far plane region of the light's frustum
    if(FragPosLightSpace.z > 1.0) shadow = 0.0;
        
    return shadow;
}

// GGX/Towbridge-Reitz normal distribution function.
// Uses Disney's reparametrization of alpha = roughness^2
float ndfGGX(float cosLh, float roughness)
{
	float alpha = roughness * roughness;
	float alphaSq = alpha * alpha;

	float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
	return alphaSq / (PI * denom * denom);
}

// Single term for separable Schlick-GGX below.
float gaSchlickG1(float cosTheta, float k)
{
	return cosTheta / (cosTheta * (1.0 - k) + k);
}

// Schlick-GGX approximation of geometric attenuation function using Smith's method.
float gaSchlickGGX(float cosLi, float NdotV, float roughness)
{
	float r = roughness + 1.0;
	float k = (r * r) / 8.0; // Epic suggests using this roughness remapping for analytic lights.
	return gaSchlickG1(cosLi, k) * gaSchlickG1(NdotV, k);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Shlick's approximation of the Fresnel factor.
vec3 fresnelSchlick(vec3 F0, float cosTheta)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlickRoughness(vec3 F0, float cosTheta, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
} 

vec3 reconstructPosition(in vec2 uv, in float depth, in mat4 InvVP) {
  float x = uv.x * 2.0f - 1.0f;
  float y = (uv.y) * 2.0f - 1.0f; // uv.y * -1 for d3d
  float z = depth * 2.0 - 1.0f;
  vec4 position_s = vec4(x, y, z, 1.0f);
  vec4 position_v = InvVP * position_s;
  vec3 div = position_v.xyz / position_v.w;
  return div;
}

void main() {
    VoxelDimensions = textureSize(voxels, 0).x;
	vec4 albedo = texture(gColors, uv);

    float depth = texture(gDepth, uv).r;
    position = reconstructPosition(uv, depth, invViewProjection);

    vec4 metallicRoughness = texture(gMetallicRoughness, uv);
    float metalness = metallicRoughness.r;
    float roughness = metallicRoughness.g;

	normal = normalize(texture(gNormals, uv).xyz);


    vec4 depthPosition = ubo.lightSpaceMatrix * vec4(position, 1.0);
    depthPosition.xyz = depthPosition.xyz * 0.5 + 0.5;

    DirectionalLight light = ubo.dirLights[0];

    //float shadowAmount = texture(shadowMap, vec3(depthPosition.xy, (depthPosition.z)/depthPosition.w));
    float shadowAmount = 1.0 - getShadow(light, position);


    vec3 Li = normalize(-light.direction.xyz);
    vec3 V = normalize(ubo.cameraPosition.xyz - position.xyz);
    vec3 Lh = normalize(Li + V);

    // Calculate angles between surface normal and various light vectors.
    float NdotV = max(dot(normal, V), 0.0);
    float cosLi = max(0.0, dot(normal, Li));
    float cosLh = max(0.0, dot(normal, Lh));

	vec3 F0 = mix(vec3(0.04), albedo.rgb, metalness);
    vec3 Fresnel = fresnelSchlick(F0, max(dot(Lh, V), 0.0));
    float D = ndfGGX(cosLh, roughness);
    float G = gaSchlickGGX(cosLi, NdotV, roughness);

    vec3 kd = (1.0 - Fresnel) * (1.0 - metalness);
    kd = kd * shadowAmount;

    // Cook-Torrance
    vec3 specularBRDF = (Fresnel * D * G) / max(0.00001, 4.0 * cosLi * NdotV);

    // get direct light
    vec3 directLight = kd * light.color.xyz * cosLi;

    // get first bounce light
    float occlusion = 0.0; 
    vec4 indirectLight = coneTraceRadiance(position, normal.xyz, occlusion);

    float reflOcclusion;
    vec4 tracedReflection = coneTraceReflection(position, normal, V,  roughness, reflOcclusion);

    // combine all
    vec3 diffuseReflection = (directLight + indirectLight.rgb * occlusion) * albedo.rgb;



    finalColor = vec4(diffuseReflection, albedo.a);

    if(depth >= 1.0) {
        finalColor = albedo;
    }

    // BLOOM SEPERATION
	float brightness = dot(finalColor.rgb, bloomThreshold);
    bloomColor = finalColor * brightness;
}
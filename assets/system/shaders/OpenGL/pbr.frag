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
layout (binding = 0) uniform stuff {
	mat4 view, projection;
	mat4 shadowMatrices[4];
	vec4 shadowSplits;
    vec4 cameraPosition;
    DirectionalLight dirLights[MAX_DIR_LIGHTS];
    PointLight pointLights[MAX_POINT_LIGHTS];
} ubo;

layout(binding = 1) uniform random {
    mat4 invViewProjection;
    vec4 bloomThreshold;
    float voxelsWorldSize;
    int pointLightCount;
    int directionalLightCount;
};

// in vars
layout(location = 0) in vec2 uv;

// output data back to our openGL program
layout (location = 0) out vec4 finalColor;
layout (location = 1) out vec4 bloomColor;

// shadow maps
layout(binding = 0) uniform sampler2DArrayShadow shadowMap;

// GBUFFER textures
layout(binding = 3) uniform sampler2D gColors;
layout(binding = 4) uniform sampler2D gNormals;
layout(binding = 6) uniform sampler3D voxels;
layout(binding = 7) uniform sampler2D gMetallicRoughness;
layout(binding = 8) uniform sampler2D gDepth;
layout(binding = 9) uniform sampler2D brdfLUT;
layout(binding = 10) uniform samplerCube environmentCubemap;
layout(binding = 11) uniform samplerCube convolvedCubemap;

float saturate(float a) { return clamp(a, 0.0, 1.0); }
bool is_saturated(float a) { return a == saturate(a); }
bool is_saturated(vec3 a) { return a.x == saturate(a.x) &&  a.y == saturate(a.y) &&  a.z == saturate(a.z); }

vec2 hammersley2d(uint idx, uint num) {
	uint bits = idx;
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	const float radicalInverse_VdC = float(bits) * 2.3283064365386963e-10; // / 0x100000000

	return vec2(float(idx) / float(num), radicalInverse_VdC);
}

vec3 importance_sample(float u, float v) {
	float phi = v * 2 * PI;
	float cosTheta = sqrt(1 - u);
	float sinTheta = sqrt(1 - cosTheta * cosTheta);
	return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

vec4 coneTrace(in vec3 p, in vec3 n, in vec3 coneDirection, in float coneAperture, out float occlusion) {
    vec4 colour = vec4(0);
    occlusion = 0.0;

    int VoxelDimensions = textureSize(voxels, 0).x;

    float voxelSize = voxelsWorldSize / VoxelDimensions;
     // start one voxel away from the current vertex' position
    float dist = voxelSize; 
    vec3 startPos = p + n * voxelSize * 2 * sqrt(2); 

    while(dist < voxelsWorldSize && colour.a < 1.0) {
        
        float diameter = max(voxelSize, 2 * coneAperture * dist);
        float mip = log2(diameter / voxelSize);

        // create vec3 for reading voxel texture from world vector
        vec3 offset = vec3(1.0 / VoxelDimensions, 1.0 / VoxelDimensions, 0);
        vec3 uv3d = (startPos + dist * coneDirection) / (voxelsWorldSize * 0.5);
        uv3d = uv3d * 0.5 + 0.5 + offset;
        vec4 sampled = textureLod(voxels, uv3d, mip);

        dist += diameter * 0.5;

        if(!is_saturated(uv3d) || dist >= voxelsWorldSize) {
            sampled.rgb = texture(environmentCubemap, normalize( coneDirection)).rgb;

            // back-to-front alpha 
            float a = (1.0 - colour.a);
            colour.rgb += a * sampled.rgb;
            colour.a += a * sampled.a;
            occlusion += (a * sampled.a) / (1.0 + 0.03 * diameter);

            break;
        }

        // back-to-front alpha 
        float a = (1.0 - colour.a);
        colour.rgb += a * sampled.rgb;
        colour.a += a * sampled.a;
        occlusion += (a * sampled.a) / (1.0 + 0.03 * diameter);

        // move along the ray


    }

    return colour;
}

mat3 GetTangentSpace(in vec3 normal){
	// Choose a helper vector for the cross product
	vec3 helper = abs(normal.x) > 0.99 ? vec3(0, 0, 1) : vec3(1, 0, 0);

	// Generate vectors
	vec3 tangent = normalize(cross(normal, helper));
	vec3 binormal = normalize(cross(normal, tangent));
	return mat3(tangent, binormal, normal);
}

vec4 coneTraceRadiance(in vec3 p, in vec3 n, int rayCount, out float occlusion_out) {
    vec4 color = vec4(0);
    occlusion_out = 0.0;

    mat3 tangentSpace = GetTangentSpace(n);

    for(int i = 0; i < rayCount; i++) {

        vec2 ham = hammersley2d(i, rayCount);
        vec3 hemi = importance_sample(ham.x, ham.y);
        vec3 coneDirection = hemi * tangentSpace;

        float occlusion = 0.0;

        // aperture is half tan of angle
        color += coneTrace(p, n, coneDirection, tan(PI * 0.5f * 0.33f), occlusion);
        occlusion_out += occlusion;
    }

    occlusion_out /= rayCount;
    occlusion_out = 1.0 - occlusion_out;

    color /= rayCount;
    color.a = clamp(color.a, 0, 1);

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

    vec2 texelSize = 1.0 / textureSize(shadowMap, 0).xy;

    for(int i = 0; i < samplesCount; i ++) {
        vec2 offsetUv = VogelDiskSample(i, samplesCount, gradientNoise) * penumbraFilterMaxSize;

        float sampleDepth = texture(shadowMap, vec4(FragPosLightSpace.xy, FragPosLightSpace.z, 0)).r; 

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
    uint cascadeIndex = 0;
    vec4 viewPos = ubo.view * vec4(position, 1.0);
    for(uint i = 0; i < 4 - 1; i++) {
        if(viewPos.z < ubo.shadowSplits[i]) {	
            cascadeIndex = i + 1;
        }
    }

    vec4 FragPosLightSpace = ubo.shadowMatrices[cascadeIndex] * vec4(position, 1.0);
    FragPosLightSpace.xyz /= FragPosLightSpace.w;
    FragPosLightSpace.xyz = FragPosLightSpace.xyz * 0.5 + 0.5;

    vec2 texelSize = 1.0 / textureSize(shadowMap, 0).xy;

    const int KernelSize = 4;
    vec2 penumbraFilterMaxSize = VogelDiskScale(textureSize(shadowMap, 0).xy, KernelSize);

    float gradientNoise = PI * 2.0 * InterleavedGradientNoise(gl_FragCoord.xy);
    int samplesCount = 16;

    float penumbra = Penumbra(gradientNoise, FragPosLightSpace, samplesCount);

    float shadow = 0.0;
    for(int i = 0; i < samplesCount; i++) {
        vec2 offsetUv = VogelDiskSample(i, samplesCount, gradientNoise);
        //offsetUv = offsetUv * penumbra;

        float sampled = texture(shadowMap, vec4(FragPosLightSpace.xy + offsetUv * texelSize, cascadeIndex, FragPosLightSpace.z)).r;
        shadow += 1.0 - sampled; 
    }

    shadow /= samplesCount;
    
    // keep the shadow at 0.0 when outside the far plane region of the light's frustum
    if(FragPosLightSpace.z > 1.0) shadow = 0.0;
        
    return shadow;
}

// GGX/Towbridge-Reitz normal distribution function.
// Uses Disney's reparametrization of alpha = roughness^2
float ndfGGX(float cosLh, float roughness) {
	float alpha = roughness * roughness;
	float alphaSq = alpha * alpha;

	float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
	return alphaSq / (PI * denom * denom);
}

// Single term for separable Schlick-GGX below.
float gaSchlickG1(float cosTheta, float k) {
	return cosTheta / (cosTheta * (1.0 - k) + k);
}

// Schlick-GGX approximation of geometric attenuation function using Smith's method.
float gaSchlickGGX(float cosLi, float NdotV, float roughness) {
	float r = roughness + 1.0;
	float k = (r * r) / 8.0; // Epic suggests using this roughness remapping for analytic lights.
	return gaSchlickG1(cosLi, k) * gaSchlickG1(NdotV, k);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
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
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
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

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

struct Material {
    vec3 albedo;
    float metallic;
    float roughness;
};

vec3 radiance(DirectionalLight light, vec3 N, vec3 V, Material material, float shadow) {
    vec3 Wi = normalize(-light.direction.xyz);
    vec3 Lh = normalize(V + Wi);

	vec3 F0 = mix(vec3(0.04), material.albedo, material.metallic);

    float NDF = DistributionGGX(N, Lh, material.roughness);   
    float G   = GeometrySmith(N, V, Wi, material.roughness);    
    vec3 F    = fresnelSchlick(max(dot(Lh, V), 0.0), F0);

    vec3 nominator    = NDF * G * F;
    float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, Wi), 0.0) + 0.001;
    vec3 specular = nominator / denominator;

    vec3 kS = F;

    vec3 kD = vec3(1.0) - kS;

    kD *= 1.0 - material.metallic;

    float NdotL = max(dot(N, Wi), 0.0);

    vec3 diffuse = light.color.rgb * NdotL * shadow;

    return (kD *  material.albedo / PI + (specular * shadow)) * diffuse;
}

vec3 ambient(DirectionalLight light, vec3 P,  vec3 N, vec3 V, Material material, float shadow) {
	vec3 F0 = mix(vec3(0.04), material.albedo, material.metallic);
    vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, material.roughness);
    
    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - material.metallic;	  
    
    vec3 R = reflect(-V, N);

    float d = clamp(dot(N, vec3(0, 1, 0)), 0.0, 1);

    // vec3 transmittance;
    // vec3 scattered = IntegrateScattering(P, vec3(0, -1, 0), INFINITY, light.direction.xyz, light.color.rgb, transmittance);

    float occlusion;
    vec4 irradiance = coneTraceRadiance(P, N, 8, occlusion);

    float occ;
    vec4 refl = coneTraceReflection(P, N, V, material.roughness, occ);

    vec3 diffuse = irradiance.rgb * material.albedo * occlusion;

    vec2 brdf  = texture(brdfLUT, vec2(max(dot(N, V), 0.0), material.roughness)).rg;

    vec3 specular = refl.rgb * (F * brdf.x + brdf.y) * occlusion;

    vec3 ambient = (kD * diffuse + specular);

    return ambient;
}

float linearize_depth(float d,float zNear,float zFar)
{
    float z_n = 2.0 * d - 1.0;
    return 2.0 * zNear * zFar / (zFar + zNear - z_n * (zFar - zNear));
}

float ScreenSpaceShadow(vec3 worldPos, DirectionalLight light) {
    vec4 rayPos = ubo.view * vec4(worldPos, 1.0);
    vec4 rayDir = ubo.view * vec4(-light.direction.xyz, 0.0);
    
    vec3 rayStep = rayDir.xyz * (0.003/ 8.0);
    float occlusion = 0.0f;

    float offset = InterleavedGradientNoise(gl_FragCoord.xy);

    for(int i = 0; i < 8; i++) {
        rayPos.xyz += rayStep;

        // get the uv coordinates
        vec4 v = ubo.projection * vec4(rayPos.xyz, 1.0); // project
        vec2 rayUV = v.xy / v.w; // perspective divide
        rayUV = rayUV * vec2(0.5) + 0.5; // -1, 1 to 0, 1

        if(rayUV.x < 1.0 && rayUV.x > 0.0 && rayUV.y < 1.0 && rayUV.y > 0.0) {
            // sample current ray point depth and convert it bback  to -1 , 1
            vec3 wpos = reconstructPosition(rayUV, texture(gDepth, rayUV).r, invViewProjection);
            vec4 vpos = ubo.view * vec4(wpos, 1.0);

            // delta between scene and ray depth
            float delta = rayPos.z - vpos.z;

            // if the scene depth is larger than the ray depth, we found an occluder
            if((vpos.z > rayPos.z) && (abs(delta) < 0.005)) {
                occlusion = 1.0;
                break;
            }
        }
    }

    return 1.0 - occlusion;
}


void main() {
    float depth = texture(gDepth, uv).r;
    vec3 position = reconstructPosition(uv, depth, invViewProjection);
    
	vec4 albedo = texture(gColors, uv);

    if(depth >= 1.0) {
        vec4 ray_clip = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
        vec3 ray_dir = (invViewProjection * ray_clip).xyz;
        finalColor = texture(environmentCubemap, normalize(ray_dir));
        
        float brightness = dot(finalColor.rgb, bloomThreshold.rgb);
        bloomColor = finalColor * min(brightness, 1.0);
        bloomColor.r = min(bloomColor.r, 1.0);
        bloomColor.g = min(bloomColor.g, 1.0);
        bloomColor.b = min(bloomColor.b, 1.0);
        return;
    }

    vec4 metallicRoughness = texture(gMetallicRoughness, uv);

    Material material;
    material.albedo = albedo.rgb;
    material.metallic = metallicRoughness.b;
    material.roughness = metallicRoughness.g;

    float metalness = metallicRoughness.r;
    float roughness = metallicRoughness.g;

	vec3 normal = normalize(texture(gNormals, uv).xyz);

    uint cascadeIndex = 0;
    vec4 viewPos = ubo.view * vec4(position, 1.0);
	for(uint i = 0; i < 4 - 1; i++) {
		if(viewPos.z < ubo.shadowSplits[i]) {	
			cascadeIndex = i + 1;
		}
	}

    vec4 depthPosition = ubo.shadowMatrices[cascadeIndex] * vec4(position, 1.0);
    depthPosition.xyz = depthPosition.xyz * 0.5 + 0.5;

    DirectionalLight light = ubo.dirLights[0];

    //float shadowAmount = texture(shadowMap, vec4(depthPosition.xy, cascadeIndex, (depthPosition.z)/depthPosition.w));
    float shadowAmount = 1.0 - getShadow(light, position);

    vec3 V = normalize(ubo.cameraPosition.xyz - position.xyz);
    vec3 Lo = radiance(light, normal, V, material, shadowAmount);

    vec3 ambient = ambient(light, position,  normal, V, material, shadowAmount);

    float occlusion;

    float I = max(dot(-light.direction.xyz, normal), 0);

    finalColor = vec4(Lo + ambient, 1.0);

    // switch(cascadeIndex) {
    //     case 0 : 
    //         finalColor.rgb *= vec3(1.0f, 0.25f, 0.25f);
    //         break;
    //     case 1 : 
    //         finalColor.rgb *= vec3(0.25f, 1.0f, 0.25f);
    //         break;
    //     case 2 : 
    //         finalColor.rgb *= vec3(0.25f, 0.25f, 1.0f);
    //         break;
    //     case 3 : 
    //         finalColor.rgb *= vec3(1.0f, 1.0f, 0.25f);
    //         break;
    // }

    // BLOOM SEPERATION
    // TODO: upscale final bloom texture to screen res and lerp between final and bloom in the tonemap shader
    // this way we can get rid of thresholding
	float brightness = dot(finalColor.rgb, bloomThreshold.rgb);
    bloomColor = finalColor * min(brightness, 1.0);
    bloomColor.r = min(bloomColor.r, 1.0);
    bloomColor.g = min(bloomColor.g, 1.0);
    bloomColor.b = min(bloomColor.b, 1.0);
}
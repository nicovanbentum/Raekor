#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#define M_PI 3.14159265358979323846
#define ONE_OVER_M_PI 0.31830988618

#include "include/structs.glsl"
#include "include/random.glsl"
#include "include/sky.glsl"

layout(push_constant) uniform pushConstants {
    mat4 invViewProj;
    vec4 cameraPosition;
    vec4 lightDir;
    uint frameCounter;
    uint maxBounces;
    float sunConeAngle;
};

layout(set = 0, binding = 1) uniform accelerationStructureEXT TLAS;

hitAttributeEXT vec2 hitAttribute;

layout(set = 0, binding = 2, std430) buffer Instances {
    Instance instances[];
};

layout(set = 0, binding = 3, std430) buffer Materials {
    Material materials[];
};

layout(buffer_reference, scalar) buffer VertexBuffer {
    Vertex data[]; 
};

layout(buffer_reference, scalar) buffer IndexBuffer {
    ivec3 data[];
};

layout(location = 0) rayPayloadInEXT RayPayload rpd;
layout(location = 1) rayPayloadEXT bool canReachLight;

layout(set = 1, binding = 0) uniform sampler2D textures[];

Vertex getInterpolatedVertex(Vertex v0, Vertex v1, Vertex v2, vec3 barycentrics) {
    Vertex vertex;
    vertex.uv = v0.uv * barycentrics.x + v1.uv * barycentrics.y + v2.uv * barycentrics.z;
    vertex.pos = v0.pos * barycentrics.x + v1.pos * barycentrics.y + v2.pos * barycentrics.z;
    vertex.normal = v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z;
    vertex.tangent = v0.tangent * barycentrics.x + v1.tangent * barycentrics.y + v2.tangent * barycentrics.z;
    
    return vertex;
}

vec3 getNormalFromTexture(Instance instance, Vertex vertex, vec4 sampledNormal) {
    vec3 normal = normalize(mat3(transpose(inverse(instance.localToWorldTransform))) * vertex.normal);
    vec3 tangent = normalize(vec3(instance.localToWorldTransform * vec4(vertex.tangent, 0.0)));
    tangent = normalize(tangent - dot(tangent, normal) * normal);

    vec3 bitangent = cross(normal, tangent);

    mat3 TBN = mat3(tangent, bitangent, normal);

    return TBN * (sampledNormal.xyz * 2.0 - 1.0);
}

Surface getSurface(Instance instance, Vertex vertex) {
    Material material = materials[instance.materialIndex.x];

    Surface surface;
    surface.albedo = material.albedo;
    surface.metallic = material.properties.x;
    surface.roughness = material.properties.y;
    surface.pos = vec3(gl_ObjectToWorldEXT * vec4(vertex.pos, 1.0));

    int albedoIndex = material.textures.x;
    int normalIndex = material.textures.y;
    int metalRoughIndex = material.textures.z;

    if(albedoIndex > -1) {
        surface.albedo *= texture(textures[albedoIndex], vertex.uv);
    }

    if(normalIndex > -1) {
        surface.normal = getNormalFromTexture(instance, vertex, texture(textures[normalIndex], vertex.uv));
    } else {
        surface.normal = normalize(mat3(transpose(inverse(instance.localToWorldTransform))) * vertex.normal);
    }

    if(metalRoughIndex > -1) {
        vec4 sampledMetallicRoughness = texture(textures[metalRoughIndex], vertex.uv);
        surface.metallic *= sampledMetallicRoughness.b;
        surface.roughness *= sampledMetallicRoughness.g;
    }

    return surface;
};

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = M_PI * denom * denom;

    return nom / (denom + 0.001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / (denom + 0.001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

vec3 evaluateLight(Surface surface, vec3 lightDirection, vec3 lightColor) {
    vec3 V = normalize(-gl_WorldRayDirectionEXT);

    vec3 Wi = normalize(-lightDirection.xyz);
    vec3 Wh = normalize(V + Wi);

    vec3 F0 = mix(vec3(0.04), surface.albedo.rgb, surface.metallic);

    float NDF = DistributionGGX(surface.normal, Wh, surface.roughness);   
    float G   = GeometrySmith(surface.normal, V, Wi, surface.roughness);    
    vec3 F    = fresnelSchlick(max(dot(Wh, V), 0.0), F0);

    vec3 nominator    = NDF * G * F;
    float denominator = 4 * max(dot(surface.normal, V), 0.0) * max(dot(surface.normal, Wi), 0.0) + 0.001;
    vec3 specular = nominator / denominator;

    vec3 kS = F;

    vec3 kD = vec3(1.0) - kS;

    kD *= 1.0 - surface.metallic;

    float NdotL = max(dot(Wi, surface.normal), 0.0);

    vec3 radiance = lightColor * NdotL;

    return (surface.albedo.rgb / M_PI) * radiance;
}

float luminance(vec3 rgb) {
	return dot(rgb, vec3(0.2126, 0.7152, 0.0722));
}

vec3 BRDF_Diffuse_Lambert(Surface surface) {
    return surface.albedo.rgb / M_PI;
}

void main() {
    Instance instance = instances[gl_InstanceCustomIndexEXT];
    IndexBuffer indices = IndexBuffer(instance.indexBufferDeviceAddress);
    VertexBuffer vertices = VertexBuffer(instance.vertexBufferDeviceAddress);

    const vec3 barycentrics = vec3(1.0 - hitAttribute.x - hitAttribute.y, hitAttribute.x, hitAttribute.y);

    ivec3 index = indices.data[gl_PrimitiveID];

    Vertex v0 = vertices.data[index.x];
    Vertex v1 = vertices.data[index.y];
    Vertex v2 = vertices.data[index.z];

    Vertex vertex = getInterpolatedVertex(v0, v1, v2, barycentrics);

    Surface surface = getSurface(instance, vertex);

    bool specularBounce = false;

    // TODO: Add emissive
    if(rpd.depth == 0 || specularBounce) {
    
    }

    // sample illumination from 1 random light
    // TODO: In this case, its just a single directional light


    // sample BRDF to get the new ray direction


//    vec2 rng = vec2(pcg_float(rpd.rng), pcg_float(rpd.rng));
//    vec2 diskPoint = uniformSampleDisk(rng.xy, sunConeAngle);
//    vec3 offsetLightDir = lightDir.xyz + vec3(diskPoint.x, 0.0, diskPoint.y);
//    
//    canReachLight = false; 
//
//    float tMin = 0.001;
//    float tMax = 10000.0;
//    uint rayFlags = gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT;
//    traceRayEXT(TLAS, rayFlags, 0xFF, 0, 0, 1, surface.pos, tMin, -offsetLightDir, tMax, 1);

    // flip the normal incase we hit a backface
    surface.normal = faceforward(surface.normal, gl_WorldRayDirectionEXT, surface.normal);

    // Importance sample the diffuse hemisphere
    rpd.rayDir = normalize(surface.normal + cosineWeightedSampleHemisphere(pcg_vec2(rpd.rng)));

    rpd.beta = surface.albedo.rgb;
    rpd.rayPos = offsetRay(surface.pos, surface.normal);
}
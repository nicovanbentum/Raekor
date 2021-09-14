#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "include/structs.glsl"
#include "include/random.glsl"

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

layout(location = 0) rayPayloadInEXT RayPayload payload;
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
    vec4 sampledAlbedo = texture(textures[material.textures.x], vertex.uv);
    vec4 sampledNormal = texture(textures[material.textures.y], vertex.uv);
    vec4 sampledMetallicRoughness = texture(textures[material.textures.z], vertex.uv);

    Surface surface;
    surface.pos = vec3(instance.localToWorldTransform * vec4(vertex.pos, 1.0));
    surface.normal = getNormalFromTexture(instance, vertex, sampledNormal);
    surface.albedo = textureLod(textures[material.textures.x], vertex.uv, 0.0);
    surface.metallic = sampledMetallicRoughness.b * material.properties.x;
    surface.roughness = sampledMetallicRoughness.g * material.properties.y;

    return surface;
};

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

    uint rayFlags = gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT;
    float tMin = 0.01;
    float tMax = 1000.0;

    canReachLight = false; 
    vec3 lightDir = normalize(vec3(0.1, 1, 0.2));

    vec4 rng = rand(payload.rng);
    vec2 unitDiskOffset = UniformSampleDisk(rng.xy);

    lightDir.x += unitDiskOffset.x;
    lightDir.z += unitDiskOffset.y;

    traceRayEXT(TLAS, rayFlags, 0xFF, 0, 0, 1, surface.pos, tMin, lightDir, tMax, 1);

    payload.Lio = vec3(surface.albedo  * uint(canReachLight) );
}
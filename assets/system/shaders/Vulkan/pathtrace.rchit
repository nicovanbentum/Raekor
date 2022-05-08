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

    Surface surface;
    surface.albedo = material.albedo;
    surface.metallic = material.properties.x;
    surface.roughness = material.properties.y;
    surface.emissive = material.emissive.rgb;
    surface.pos = vec3(gl_ObjectToWorldEXT * vec4(vertex.pos, 1.0));

    int albedoIndex = material.textures.x;
    int normalIndex = material.textures.y;
    int metalRoughIndex = material.textures.z;

    if(albedoIndex > -1) {
        surface.albedo *= texture(textures[albedoIndex], vertex.uv);
    }

    if(normalIndex > -1) {
        surface.shadingNormal = getNormalFromTexture(instance, vertex, texture(textures[normalIndex], vertex.uv));
    } else {
        surface.shadingNormal = normalize(mat3(transpose(inverse(instance.localToWorldTransform))) * vertex.normal);
    }

    if(metalRoughIndex > -1) {
        vec4 sampledMetallicRoughness = texture(textures[metalRoughIndex], vertex.uv);
        surface.metallic *= sampledMetallicRoughness.b;
        surface.roughness *= sampledMetallicRoughness.g;
    }

    // flip the normal incase we hit a backface
    //surface.normal = faceforward(surface.normal, gl_WorldRayDirectionEXT, surface.normal);

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


float luminance(vec3 rgb) {
	return dot(rgb, vec3(0.2126, 0.7152, 0.0722));
}


/* Returns Wh. From Unreal Engine 4 https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf */
vec3 ImportanceSampleGGX(vec2 Xi, float Roughness , vec3 N) {
    float a = Roughness * Roughness;
    float Phi = 2 * PI * Xi.x;
    float CosTheta = sqrt( (1 - Xi.y) / ( 1 + (a*a - 1) * Xi.y ) );
    float SinTheta = sqrt( 1 - CosTheta * CosTheta );
    vec3 H;
    H.x = SinTheta * cos( Phi );
    H.y = SinTheta * sin( Phi );
    H.z = CosTheta;
    vec3 UpVector = abs(N.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
    vec3 TangentX = normalize( cross( UpVector , N ) );
    vec3 TangentY = cross( N, TangentX );
    // Tangent to world space
    return TangentX * H.x + TangentY * H.y + N * H.z;
}

/* From https://google.github.io/filament/Filament.html#materialsystem/specularbrdf/fresnel(specularf) */
vec3 F_Schlick(float u, vec3 f0) {
    float f = pow(1.0 - u, 5.0);
    return f + f0 * (1.0 - f);
}

/* Evaluates diffuse + specular BRDF */
vec3 BRDF_Eval(Surface surface, vec3 Wo, vec3 Wi, vec3 Wh) {
    // these aliases are here just so my brain can follow along
    vec3 V = Wo;
    vec3 L = Wi;
    vec3 H = Wh;
    vec3 N = surface.shadingNormal;

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

	vec3 F0 = mix(vec3(0.04), surface.albedo.rgb, surface.metallic);
    vec3 F    = F_Schlick(VdotH, F0);

    float NDF = DistributionGGX(N, Wh, surface.roughness);   
    float G   = GeometrySmith(N, V, L, surface.roughness);    

    vec3 nominator    = NDF * G * F;
    float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;

    // lambertian diffuse, the whole equation cancels out to just albedo
    vec3 diffuse = ((1.0 - surface.metallic) * surface.albedo.rgb);
    vec3 specular = nominator / denominator;

    return (1.0 - F) * diffuse + specular;
}

float Smith_G1_GGX(float alpha, float NdotL, float alphaSquared, float NdotLSquared) {
	return 2.0 / (sqrt(((alphaSquared * (1.0 - NdotLSquared)) + NdotLSquared) / NdotLSquared) + 1.0);
}


/* Returns the BRDF value, also outputs new outgoing direction and pdf */
vec3 BRDF_Sample(Surface surface, vec3 Wo, out vec3 Wi, out vec3 weight) {
    vec3 Wh;
    const float rand = pcg_float(payload.rng);

    // randomly decide to specular bounce
    if (rand > 0.5 && surface.roughness < 0.5) {
        // Importance sample the specular lobe using UE4's example to get the half vector
        Wh = ImportanceSampleGGX(pcg_vec2(payload.rng), surface.roughness, surface.shadingNormal);
        Wi = normalize(reflect(-Wo, Wh));

        float VdotH = clamp(dot(Wo, Wh), 0.0001, 1.0);
        float NdotL = clamp(dot(surface.shadingNormal, Wi), 0.0001, 1.0);

        float alpha = surface.roughness * surface.roughness;
        vec3 F0 = mix(vec3(0.04), surface.albedo.rgb, surface.metallic);
        weight = F_Schlick(VdotH, F0) * Smith_G1_GGX(alpha, NdotL, alpha * alpha, NdotL * NdotL);
    } else {
        // importance sample the hemisphere around the normal for diffuse
        Wi = normalize(surface.shadingNormal + cosineWeightedSampleHemisphere(pcg_vec2(payload.rng)));
        Wh = normalize(Wo + Wi);
        weight = ((1.0 - surface.metallic) * surface.albedo.rgb);
    }

    return BRDF_Eval(surface, Wo, Wi, Wh);
}


vec3 SampleSunlight(Surface surface, vec3 direction, out vec3 Wi) {
    vec2 diskPoint = uniformSampleDisk(pcg_vec2(payload.rng), sunConeAngle);
    Wi = -(direction + vec3(diskPoint.x, 0.0, diskPoint.y));

    float tMin = 0.001;
    float tMax = 1000.0;
    uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT;

    // Offset pos along the surface normal, from Ray Tracing Gems 1
    vec3 offset_surface_pos = offsetRay(surface.pos.xyz, surface.triangleNormal);

    canReachLight = false; 
    traceRayEXT(TLAS, rayFlags, 0xFF, 0, 0, 1, offset_surface_pos, tMin, Wi, tMax, 1);

    vec3 light_color = Absorb(IntegrateOpticalDepth(vec3(0.0), direction));
    return light_color * uint(canReachLight);
}

void main() {
    Instance instance = instances[gl_InstanceCustomIndexEXT];
    IndexBuffer indices = IndexBuffer(instance.indexBufferDeviceAddress);
    VertexBuffer vertices = VertexBuffer(instance.vertexBufferDeviceAddress);

    const vec3 barycentrics = vec3(1.0 - hitAttribute.x - hitAttribute.y, hitAttribute.x, hitAttribute.y);

    const ivec3 index = indices.data[gl_PrimitiveID];
    const Vertex v0 = vertices.data[index.x];
    const Vertex v1 = vertices.data[index.y];
    const Vertex v2 = vertices.data[index.z];

    const Vertex vertex = getInterpolatedVertex(v0, v1, v2, barycentrics);
    Surface surface = getSurface(instance, vertex);
    surface.triangleNormal = normalize(cross(v0.pos - v1.pos, v0.pos - v2.pos));
    // furnace test: surface.albedo = vec4(1.0);

    // emissive is just a flat addition to L
    payload.L = surface.emissive;

    vec3 Wo = -gl_WorldRayDirectionEXT;

    { // Evaluate BRDF for sunlight and add it to total radiance L
        const vec3 light_dir = normalize(lightDir.xyz);

        vec3 Wi;
        const vec3 sunlight = SampleSunlight(surface, light_dir, Wi);
        const float NdotL = max(dot(surface.shadingNormal, Wi), 0.0);

        vec3 Wh = normalize(Wo + Wi);

        const vec3 brdf = BRDF_Eval(surface, Wo, Wi, Wh);

        payload.L += brdf * NdotL * sunlight;
    }

    { // Sample BRDF for new outgoing direction and update throughput
        const vec3 brdf = BRDF_Sample(surface, Wo, payload.rayDir, payload.beta);
        payload.rayPos = offsetRay(surface.pos, surface.triangleNormal);
    }

    // // Russian roulette
    if (payload.depth > 3) {
        const float r = pcg_float(payload.rng);
        const float p = max(surface.albedo.r, max(surface.albedo.g, surface.albedo.b));
        if(r > p) {
            payload.depth = 100;
        } else {
            payload.beta = vec3(1.0 / p);
        }
    }
}
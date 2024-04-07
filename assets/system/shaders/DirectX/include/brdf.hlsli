#ifndef BRDF_HLSLI
#define BRDF_HLSLI

#include "shared.h"
#include "sky.hlsli"
#include "packing.hlsli"
#include "bindless.hlsli"
#include "random.hlsli"

float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.14159265359 * denom * denom;

    return nom / (denom + 0.001);
}


float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / (denom + 0.001);
}


float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}


/* From https://google.github.io/filament/Filament.html#materialsystem/specularbrdf/fresnel(specularf) */
float3 F_Schlick(float u, float3 f0) {
    float f = pow(1.0 - u, 5.0);
    return f + f0 * (1.0 - f);
}


/* Returns Wh. From Unreal Engine 4 https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf */
float3 ImportanceSampleGGX(float2 Xi, float Roughness, float3 N) {
    float a = Roughness * Roughness;
    float Phi = 2 * M_PI * Xi.x;
    float CosTheta = sqrt((1 - Xi.y) / (1 + (a * a - 1) * Xi.y));
    float SinTheta = sqrt(1 - CosTheta * CosTheta);
    float3 H;
    H.x = SinTheta * cos(Phi);
    H.y = SinTheta * sin(Phi);
    H.z = CosTheta;
    float3 UpVector = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 TangentX = normalize(cross(UpVector, N));
    float3 TangentY = cross(N, TangentX);
    // Tangent to world space
    return TangentX * H.x + TangentY * H.y + N * H.z;
}


float Smith_G1_GGX(float alpha, float NdotL, float alphaSquared, float NdotLSquared) {
    return 2.0 / (sqrt(((alphaSquared * (1.0 - NdotLSquared)) + NdotLSquared) / NdotLSquared) + 1.0);
}

float3 ReconstructNormalBC5(float2 normal)
{
    float2 xy = 2.0f * normal - 1.0f;
    float z = sqrt(1 - dot(xy, xy));
    return float3(xy.x, xy.y, z);
}

struct BRDF {
    float4 mAlbedo;
    float3 mNormal;
    float3 mEmissive;
    float mMetallic;
    float mRoughness;
    
    /* Fills in the BRDF fields from a sample of the (packed) GBuffer. */
    void Unpack(uint4 inPacked) {
        mAlbedo = UnpackAlbedo(inPacked);
        mNormal = UnpackNormal(inPacked);
        mEmissive = UnpackEmissive(inPacked);
        UnpackMetallicRoughness(inPacked, mMetallic, mRoughness);
    }
    
    
    /* Fills in the BRDF fields from a given vertex and its material. */
    void FromHit(RTVertex inVertex, RTMaterial inMaterial) {
        Texture2D albedo_texture = ResourceDescriptorHeap[NonUniformResourceIndex(inMaterial.mAlbedoTexture)];
        Texture2D normals_texture = ResourceDescriptorHeap[NonUniformResourceIndex(inMaterial.mNormalsTexture)];
        Texture2D emissive_texture = ResourceDescriptorHeap[NonUniformResourceIndex(inMaterial.mEmissiveTexture)];
        Texture2D metallic_texture = ResourceDescriptorHeap[NonUniformResourceIndex(inMaterial.mMetallicTexture)];
        Texture2D roughness_texture = ResourceDescriptorHeap[NonUniformResourceIndex(inMaterial.mRoughnessTexture)];
    
        float4 sampled_albedo = albedo_texture.Sample(SamplerPointWrapNoMips, inVertex.mTexCoord);
        float3 sampled_normal = normals_texture.Sample(SamplerPointWrapNoMips, inVertex.mTexCoord).rgb; // alpha channel unused
        float3 sampled_emissive = emissive_texture.Sample(SamplerPointWrapNoMips, inVertex.mTexCoord).rgb; // alpha channel unused
        float sampled_metallic = metallic_texture.Sample(SamplerPointWrapNoMips, inVertex.mTexCoord).r; // value swizzled across all channels, just get Red
        float sampled_roughness = roughness_texture.Sample(SamplerPointWrapNoMips, inVertex.mTexCoord).r; // value swizzled across all channels, just get Red
        
        //sampled_normal = sampled_normal * 2.0 - 1.0;
        sampled_normal = ReconstructNormalBC5(sampled_normal.xy);
        
        float3 bitangent = normalize(cross(inVertex.mNormal, inVertex.mTangent));
        float3x3 TBN = transpose(float3x3(inVertex.mTangent, bitangent, inVertex.mNormal));
        
        mAlbedo = inMaterial.mAlbedo * sampled_albedo;
        mNormal = normalize(mul(TBN, sampled_normal.xyz));
        // mNormal = inVertex.mNormal;
        mEmissive = inMaterial.mEmissive.rgb * sampled_emissive;
        mMetallic = inMaterial.mMetallic * sampled_metallic;
        mRoughness = inMaterial.mRoughness * sampled_roughness;
    }
    
    
    float3 Evaluate(float3 Wo, float3 Wi, float3 Wh) {
        float NdotL = max(dot(mNormal, Wi), 0.0);
        float NdotV = max(dot(mNormal, Wo), 0.0);
        float NdotH = max(dot(mNormal, Wh), 0.0);
        float VdotH = max(dot(Wo, Wh), 0.0);

        float3 F0 = lerp(0.04.xxx, mAlbedo.rgb, mMetallic);
        float3 F    = F_Schlick(VdotH, F0);

        float NDF = DistributionGGX(mNormal, Wh, mRoughness);
        float G = GeometrySmith(mNormal, Wo, Wi, mRoughness);

        float3 nominator    = NDF * G * F;
        float denominator = 4 * max(dot(mNormal, Wo), 0.0) * max(dot(mNormal, Wi), 0.0) + 0.001;

        // lambertian diffuse, the whole equation cancels out to just albedo
        float3 diffuse = ((1.0 - mMetallic) * mAlbedo.rgb);
        float3 specular = nominator / denominator;

        return (1.0 - F) * diffuse + specular;
    }
    
    void SampleDiffuse(inout uint rng, float3 Wo, out float3 Wi)
    {
        Wi = normalize(mNormal + SampleCosineWeightedHemisphere(pcg_float2(rng)));
    }
    
    // Sample specular lobe
    void SampleSpecular(inout uint rng, float3 Wo, out float3 Wi, out float3 Wh)
    {
        if (mRoughness == 0.0)
        {
            Wh = mNormal; // roughness 0 is a perfect reflection, so just reflect around the normal
        }
        else
        {
            // Importance sample the specular lobe using UE4's example to get the half vector
            Wh = ImportanceSampleGGX(pcg_float2(rng), mRoughness, mNormal);
        }
        Wi = normalize(reflect(-Wo, Wh));
    }
    
    
    /* Returns the BRDF value, also outputs new outgoing direction and pdf */
    void Sample(inout uint rng, float3 Wo, out float3 Wi, out float3 weight) {
        const float rand = pcg_float(rng);

        // randomly decide to specular bounce
        if (rand > 0.5) {
            float3 Wh;
            SampleSpecular(rng, Wo, Wi, Wh);

            float VdotH = clamp(dot(Wo, Wh), 0.0001, 1.0);
            float NdotL = clamp(dot(mNormal, Wi), 0.0001, 1.0);

            float alpha = mRoughness * mRoughness;
            float3 F0 = lerp(0.04.xxx, mAlbedo.rgb, mMetallic);
            weight = F_Schlick(VdotH, F0) * Smith_G1_GGX(alpha, NdotL, alpha * alpha, NdotL * NdotL);
        }
        else {
        // importance sample the hemisphere around the normal for diffuse
            SampleDiffuse(rng, Wo, Wi);
            weight = ((1.0 - mMetallic) * mAlbedo.rgb);
        }
    }
};


bool TraceShadowRay(RaytracingAccelerationStructure inTLAS, float3 inRayPos, float3 inRayDir, float inTMin, float inTMax)
{
    RayDesc shadow_ray;
    shadow_ray.Origin = inRayPos;
    shadow_ray.Direction = inRayDir;
    shadow_ray.TMin = inTMin;
    shadow_ray.TMax = inTMax;
                
    uint shadow_ray_flags = RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
    
    RayQuery < RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER > query;

    query.TraceRayInline(inTLAS, shadow_ray_flags, 0xFF, shadow_ray);
    query.Proceed();
                
    return query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}


float3 SampleDirectionalLight(float3 inLightDir, float inConeAngle, float2 inRNG)
{
    const float3 light_dir = normalize(inLightDir.xyz);
    float3 light_tangent = normalize(cross(light_dir, float3(0.0f, 1.0f, 0.0f)));
    float3 light_bitangent = normalize(cross(light_tangent, light_dir));
    
    float2 disk_point = uniformSampleDisk(inRNG, inConeAngle);
    return -normalize(light_dir + disk_point.x * light_tangent + disk_point.y * light_bitangent);
}
    

float3 EvaluateDirectionalLight(BRDF inBrdf, float4 inLightColor, float3 Wi, float3 Wo)
{             
    float3 sunlight_luminance = Absorb(IntegrateOpticalDepth(0.xxx, -Wi)) * inLightColor.a;
                
    const float NdotL = max(dot(inBrdf.mNormal, Wi), 0.0);
                
    const float3 Wh = normalize(Wo + Wi);
                
    const float3 l = inBrdf.Evaluate(Wo, Wi, Wh);
                
    return l * NdotL * sunlight_luminance;
}


float3 SamplePointLight(RTLight inLight, float3 inPosWS)
{
    return normalize(inLight.mPosition.xyz - inPosWS);
}


float GetPointAttenuation(RTLight inLight, float inDistance)
{
    float dist2 = dot(inDistance, inDistance);
    float rcp_radius = 1.0 / inLight.mAttributes.x;
    float factor = dist2 * rcp_radius * rcp_radius;
    float smoothing = max(1.0 - factor * factor, 0.0);
    return (smoothing * smoothing) / max(dist2, 1e-4);
}


float3 EvaluatePointLight(BRDF inBrdf, RTLight inLight, float3 Wi, float3 Wo, float inDistance)
{
    const float NdotL = max(dot(inBrdf.mNormal, Wi), 0.0);
    
    const float3 Wh = normalize(Wo + Wi);
    
    const float3 l = inBrdf.Evaluate(Wo, Wi, Wh);
    
    const float attenuation = GetPointAttenuation(inLight, inDistance);
    
    return l * NdotL * attenuation * (inLight.mColor.rgb * inLight.mColor.a);
}


float3 SampleSpotLight(RTLight inLight, float3 inPosWS)
{
    return normalize(inLight.mPosition.xyz - inPosWS);
}


float GetSpotAttenuation(RTLight inLight, float inDistance, float3 Wi)
{
    float cos_outer = cos(inLight.mAttributes.z);
    float spot_scale = 1.0 / max(cos(inLight.mAttributes.y) - cos_outer, 1e-4);
    float spot_offset = -cos_outer * spot_scale;

    float cd = dot(normalize(-inLight.mDirection), Wi);
    float attenuation = clamp(cd * spot_scale + spot_offset, 0.0, 1.0);
    return attenuation * attenuation;
}


float3 EvaluateSpotLight(BRDF inBrdf, RTLight inLight, float3 Wi, float3 Wo, float inDistance)
{
    const float NdotL = max(dot(inBrdf.mNormal, Wi), 0.0);
    
    const float3 Wh = normalize(Wo + Wi);
    
    const float3 l = inBrdf.Evaluate(Wo, Wi, Wh);
    
    const float attenuation = GetPointAttenuation(inLight, inDistance) * GetSpotAttenuation(inLight, inDistance, Wi);
    
    return l * NdotL * attenuation * (inLight.mColor.rgb * inLight.mColor.a);
}


#endif // BRDF_HLSLI
#ifndef BRDF_HLSLI
#define BRDF_HLSLI

#include "packing.hlsli"
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

struct BRDF {
    float4 mAlbedo;
    float3 mNormal;
    float mMetallic;
    float mRoughness;
    
    void Unpack(uint4 inPacked) {
        mAlbedo = UnpackAlbedo(inPacked.x);
        mNormal = UnpackNormal(inPacked.y);
        UnpackMetallicRoughness(inPacked.z, mMetallic, mRoughness);
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
};

#endif // BRDF_HLSLI
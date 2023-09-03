#ifndef BRDF_HLSLI
#define BRDF_HLSLI

#include "shared.h"
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


struct BRDF {
    float4 mAlbedo;
    float3 mNormal;
    float mMetallic;
    float mRoughness;
    
    /* Fills in the BRDF fields from a sample of the (packed) GBuffer. */
    void Unpack(uint4 inPacked) {
        mAlbedo = UnpackAlbedo(inPacked);
        mNormal = UnpackNormal(inPacked);
        UnpackMetallicRoughness(inPacked, mMetallic, mRoughness);
    }
    
    
    /* Fills in the BRDF fields from a given vertex and its material. */
    void FromHit(RTVertex inVertex, RTMaterial inMaterial) {
        Texture2D albedo_texture = ResourceDescriptorHeap[NonUniformResourceIndex(inMaterial.mAlbedoTexture)];
        Texture2D normals_texture = ResourceDescriptorHeap[NonUniformResourceIndex(inMaterial.mNormalsTexture)];
        Texture2D metalrough_texture = ResourceDescriptorHeap[NonUniformResourceIndex(inMaterial.mMetalRoughTexture)];
    
        float4 sampled_albedo = albedo_texture.Sample(SamplerPointWrapNoMips, inVertex.mTexCoord);
        float4 sampled_normal = normals_texture.Sample(SamplerPointWrapNoMips, inVertex.mTexCoord);
        float4 sampled_metalrough = metalrough_texture.Sample(SamplerPointWrapNoMips, inVertex.mTexCoord);
        
        float3 bitangent = normalize(cross(inVertex.mNormal, inVertex.mTangent));
        float3x3 TBN = transpose(float3x3(inVertex.mTangent, bitangent, inVertex.mNormal));
        
        mAlbedo = inMaterial.mAlbedo * sampled_albedo;
        mNormal = normalize(mul(TBN, sampled_normal.xyz * 2.0 - 1.0));
        // mNormal = inVertex.mNormal;
        mMetallic = inMaterial.mMetallic * sampled_metalrough.b;
        mRoughness = inMaterial.mRoughness * sampled_metalrough.g;
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
    
    
    /* Returns the BRDF value, also outputs new outgoing direction and pdf */
    void Sample(uint rng, float3 Wo, out float3 Wi, out float3 weight) {
        float3 Wh;
        const float rand = pcg_float(rng);

        // randomly decide to specular bounce
        if (rand > 0.5 && mRoughness < 0.5) {
        // Importance sample the specular lobe using UE4's example to get the half vector
            if (mRoughness == 0.0)
                Wh = mNormal; // roughness 0 is a perfect reflection, so we can just reflect around the normal
            else
                Wh = ImportanceSampleGGX(pcg_float2(rng), mRoughness, mNormal);

            Wi = normalize(reflect(-Wo, Wh));

            float VdotH = clamp(dot(Wo, Wh), 0.0001, 1.0);
            float NdotL = clamp(dot(mNormal, Wi), 0.0001, 1.0);

            float alpha = mRoughness * mRoughness;
            float3 F0 = lerp(0.04.xxx, mAlbedo.rgb, mMetallic);
            weight = F_Schlick(VdotH, F0) * Smith_G1_GGX(alpha, NdotL, alpha * alpha, NdotL * NdotL);
        }
        else {
        // importance sample the hemisphere around the normal for diffuse
            Wi = normalize(mNormal + SampleCosineWeightedHemisphere(pcg_float2(rng)));
            Wh = normalize(Wo + Wi);
            weight = ((1.0 - mMetallic) * mAlbedo.rgb);
        }
    }
};

#endif // BRDF_HLSLI
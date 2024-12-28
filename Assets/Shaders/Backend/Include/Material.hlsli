#ifndef BRDF_HLSLI
#define BRDF_HLSLI

#include "Sky.hlsli"
#include "Common.hlsli"
#include "Shared.hlsli"
#include "Random.hlsli"
#include "Packing.hlsli"
#include "Bindless.hlsli"

float DistributionGGX(float3 N, float3 H, float roughness) 
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = M_PI * denom * denom;

    return nom / (denom + 0.001);
}


float GeometrySchlickGGX(float NdotV, float roughness) 
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / (denom + 0.001);
}


float GeometrySmith(float3 N, float3 V, float3 L, float roughness) 
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);

    return ggx1 * ggx2;
}


// Revisit for better material control, e.g. dieletric vs conductor behavior
// 'f90' should be 1.0, TODO: specular workflow support?
float3 FresnelSchlick(float NdotV, float3 F0, float F90) 
{
    return F0 + (F90 - F0) * pow(1.0 - NdotV, 5.0);
}


// Slightly more optimized version, N should be the microfacet normal (aka half vector)
float3 FresnelSchlickUE4(float NdotV, float3 F0, float F90)
{
    return F0 + (F90 - F0) * exp2((-5.55473f * NdotV - 6.983146f) * NdotV);
}


/* Returns Wh. From Unreal Engine 4 https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf */
float3 SampleSpecularGGX(float2 Xi, float roughness, float3 N) 
{
    float a = roughness * roughness;
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


// Source: "Sampling Visible GGX Normals with Spherical Caps" 
// https://cdrdv2-public.intel.com/782052/sampling-visible-ggx-normals.pdf
// Ve is V in local space, alpha2D = (roughness, roughness), u = pcg_float2(..)
float3 SampleSpecularGGXVNDF(float3 Ve, float2 alpha2D, float2 u)
{
    float3 Vh = normalize(float3(alpha2D.x * Ve.x, alpha2D.y * Ve.y, Ve.z));

	float phi = 2.0f * PI * u.x;
	float z = ((1.0f - u.y) * (1.0f + Vh.z)) - Vh.z;
	float sinTheta = sqrt(clamp(1.0f - z * z, 0.0f, 1.0f));
	float x = sinTheta * cos(phi);
	float y = sinTheta * sin(phi);

	float3 Nh = float3(x, y, z) + Vh;

    return normalize(float3(alpha2D.x * Nh.x, alpha2D.y * Nh.y, max(0.0f, Nh.z)));
}


float Smith_G1_GGX(float alpha, float NdotL, float alphaSquared, float NdotLSquared) 
{
    return 2.0 / (sqrt(((alphaSquared * (1.0 - NdotLSquared)) + NdotLSquared) / NdotLSquared) + 1.0);
}


float3 ReconstructNormalBC5(float2 normal)
{
    float2 xy = 2.0f * normal - 1.0f;
    float z = sqrt(1 - dot(xy, xy));
    return float3(xy.x, xy.y, z);
}


struct Surface 
{
    float4 mAlbedo;
    float3 mNormal;
    float3 mEmissive;
    float  mMetallic;
    float  mRoughness;
    
    /* Fills in the BRDF fields from a sample of the (packed) GBuffer. */
    void Unpack(uint4 inPacked) 
    {
        mAlbedo = UnpackAlbedo(inPacked);
        mNormal = UnpackNormal(inPacked);
        mEmissive = UnpackEmissive(inPacked);
        UnpackMetallicRoughness(inPacked, mMetallic, mRoughness);
    }
    
    
    /* Fills in the BRDF fields from a given vertex and its material. */
    void FromHit(RTVertex inVertex, RTMaterial inMaterial) 
    {
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
        mNormal = inVertex.mNormal;
        sampled_normal = ReconstructNormalBC5(sampled_normal.xy);
        
        float3 bitangent = normalize(cross(inVertex.mNormal, inVertex.mTangent));
        float3x3 TBN = float3x3(inVertex.mTangent, bitangent, inVertex.mNormal);
        mNormal = normalize(mul(sampled_normal.xyz, TBN));
        
        mAlbedo = inMaterial.mAlbedo * sampled_albedo;
        mEmissive = inMaterial.mEmissive.rgb * sampled_emissive;
        mMetallic = inMaterial.mMetallic * sampled_metallic;
        mRoughness = inMaterial.mRoughness * sampled_roughness;
    }
    
    
    float3 EvaluateBRDF(float3 Wo, float3 Wi, float3 Wh) 
    {
        float NdotL = max(dot(mNormal, Wi), 0.0);
        float NdotV = max(dot(mNormal, Wo), 0.0);
        float NdotH = max(dot(mNormal, Wh), 0.0);
        float HdotV = max(dot(Wh, Wo), 0.0);

        float3 F0 = lerp(0.04, mAlbedo.rgb, mMetallic);
        float3 F = FresnelSchlick(HdotV, F0, 1.0);
        //float3 F = FresnelSchlickUE4(HdotV, F0, F90);
        
        float G = GeometrySmith(mNormal, Wo, Wi, mRoughness);
        float D = DistributionGGX(mNormal, Wh, mRoughness);

        float3 nominator = F * G * D;
        float denominator = 4 * NdotL * NdotV + 0.001;
        float3 specularBrdf = nominator / denominator;
      
        float3 diffuseBrdf = ((1.0 - mMetallic) * mAlbedo.rgb) * (1.0 - F);
        
        return diffuseBrdf + specularBrdf;
    }
    
    float SampleDiffusePDF(float3 Wi) // not actually used, useful for MIS
    {
        return max(dot(mNormal, Wi), 0.0) / M_PI;
    }
    
    float3 SampleDiffuseWeight(float3 Wo, float3 Wi, float3 Wh)
    {
        float NdotL = max(dot(mNormal, Wi), 0.0);
        float NdotV = max(dot(mNormal, Wo), 0.0);
        float NdotH = max(dot(mNormal, Wh), 0.0);
        float HdotV = max(dot(Wh, Wo), 0.0);
        
        float3 F0 = lerp(0.04.xxx, mAlbedo.rgb, mMetallic);
        
        float3 weight = (1.0 - mMetallic) * mAlbedo.rgb;
        return weight * (1.0 - FresnelSchlick(NdotV, F0, 1.0));
    }
    
    void SampleDiffuse(inout uint rng, float3 Wo, out float3 direction, out float3 weight)
    {
        weight = SampleDiffuseWeight(Wo, direction, mNormal);
        direction = mul(BuildOrthonormalBasis(mNormal), SampleCosineWeightedHemisphere(pcg_float2(rng)));
    }
    
    float sampleSpecularPDF(float3 Wo, float3 Wh)  // not actually used, useful for MIS
    {
        float NdotV = max(dot(mNormal, Wo), 0.00001f);
        float NdotH = max(dot(mNormal, Wh), 0.00001f);
        
        float D = DistributionGGX(mNormal, Wh, mRoughness);
        float G = Smith_G1_GGX(mRoughness, NdotV, mRoughness*mRoughness, NdotV * NdotV);
        
        return (D * G) / (4.0f * NdotV);
    }
    
    float3 SampleSpecularWeight(float3 Wo, float3 Wi, float3 Wh)
    {
        float NdotL = max(dot(mNormal, Wi), 0.00001f);
        float NdotV = max(dot(mNormal, Wo), 0.00001f);
        float NdotH = max(dot(mNormal, Wh), 0.0);
        float HdotV = max(dot(Wh, Wo), 0.0);
        
        float3 F0 = lerp(0.04, mAlbedo.rgb, mMetallic);
        float3 fresnel = FresnelSchlick(NdotV, F0, 1.0);
        return fresnel * Smith_G1_GGX(mRoughness, NdotL, mRoughness * mRoughness, NdotL * NdotL);
    }
    
    void SampleSpecular(inout uint rng, float3 Wo, out float3 direction, out float3 weight)
    {
        float3 Wh = mNormal;
        
        if (mRoughness > 0.0)
        {
            Wh = SampleSpecularGGX(pcg_float2(rng), mRoughness, mNormal);
            //Wh = SampleSpecularGGXVNDF(Wo, float2(mRoughness, mRoughness), pcg_float2(rng));
        }
        
        direction = normalize(reflect(-Wo, Wh));
        weight = SampleSpecularWeight(Wo, direction, Wh);
    }
    
    
    void SampleBRDF(inout uint rng, float3 Wo, out float3 direction, out float3 weight) 
    {
        if (mRoughness < 1.0 && pcg_float(rng) > 0.5)
        {
            SampleSpecular(rng, Wo, direction, weight);
        }
        else
        {
            SampleDiffuse(rng, Wo, direction, weight);
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
    

float3 EvaluateDirectionalLight(Surface inSurface, float4 inLightColor, float3 Wi, float3 Wo)
{             
    float3 sunlight_luminance = Absorb(IntegrateOpticalDepth(0.xxx, -Wi)) * inLightColor.a;
                
    const float NdotL = max(dot(inSurface.mNormal, Wi), 0.0);
                
    const float3 Wh = normalize(Wo + Wi);
                
    const float3 brdf = inSurface.EvaluateBRDF(Wo, Wi, Wh);
                
    return brdf * NdotL * sunlight_luminance;
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


float3 EvaluatePointLight(Surface inSurface, RTLight inLight, float3 Wi, float3 Wo, float inDistance)
{
    const float NdotL = max(dot(inSurface.mNormal, Wi), 0.0);
    
    const float3 Wh = normalize(Wo + Wi);
    
    const float3 brdf = inSurface.EvaluateBRDF(Wo, Wi, Wh);
    
    const float attenuation = GetPointAttenuation(inLight, inDistance);
    
    return brdf * NdotL * attenuation * (inLight.mColor.rgb * inLight.mColor.a);
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


float3 EvaluateSpotLight(Surface inSurface, RTLight inLight, float3 Wi, float3 Wo, float inDistance)
{
    const float NdotL = max(dot(inSurface.mNormal, Wi), 0.0);
    
    const float3 Wh = normalize(Wo + Wi);
    
    const float3 l = inSurface.EvaluateBRDF(Wo, Wi, Wh);
    
    const float attenuation = GetPointAttenuation(inLight, inDistance) * GetSpotAttenuation(inLight, inDistance, Wi);
    
    return l * NdotL * attenuation * (inLight.mColor.rgb * inLight.mColor.a);
}


#endif // BRDF_HLSLI
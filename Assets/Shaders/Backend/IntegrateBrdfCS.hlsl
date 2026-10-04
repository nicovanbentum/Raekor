#include "Include/Sky.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"
#include "Include/Material.hlsli"
#include "Include/Bindless.hlsli"

ROOT_CONSTANTS(IntegrateBrdfConstants, rc)

#define PI 3.14159265359
#define NR_OF_SAMPLES 1024

float3 sSampleSpecularGGX(float2 Xi, float roughness, float3 N)
{
    float a = roughness * roughness;
    
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    return H;
}

// 3. Height-Correlated Smith Visibility Function
// Returns G / (4 * NdotL * NdotV)
float GeometrySmithVisibility(float NdotV, float NdotL, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;

    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    
    return 0.5 / (GGXV + GGXL);
}

float2 IntegrateBRDF(float inNdotV, float inRoughness)
{
    inNdotV = max(inNdotV, 0.02);
    inRoughness = max(inRoughness, 0.04);
    
    float3 V;
    V.x = sqrt(1.0 - inNdotV * inNdotV);
    V.y = 0.0;
    V.z = inNdotV;

    float A = 0.0;
    float B = 0.0;

    float3 N = float3(0.0, 0.0, 1.0);

    for (uint i = 0u; i < NR_OF_SAMPLES; ++i)
    {
        float2 Xi = Hammersley2D(i, NR_OF_SAMPLES);
        float3 H = sSampleSpecularGGX(Xi, inRoughness, N);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);
        float NdotH = max(H.z, 0.0);

        if (NdotL > 0.0)
        {
            float Vis = GeometrySmithVisibility(inNdotV, NdotL, inRoughness);
            
            // The 4.0 and NdotL terms come from the BRDF denominator and cosine weight cancellation
            float G_Vis = (Vis * 4.0 * VdotH * NdotL) / NdotH;
            float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    return float2(A, B) / float(NR_OF_SAMPLES);
}

[numthreads(8, 8, 1)]
void main(uint3 inPixelCoord : SV_DispatchThreadID)
{
    float width, height;
    RWTexture2D<float2> output_texture = ResourceDescriptorHeap[rc.mOutputTexture];
    output_texture.GetDimensions(width, height);

    if (inPixelCoord.x >= width || inPixelCoord.y >= height)
        return;

    float2 uv = (inPixelCoord.xy + 0.5f) / float2(width, height);
    float4 color = 1.0f;
   
    color.rg = IntegrateBRDF(uv.x, uv.y);
    
    output_texture[inPixelCoord.xy] = color.rg;
}
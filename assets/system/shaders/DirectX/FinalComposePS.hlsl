#include "include/bindless.hlsli"
#include "include/common.hlsli"
#include "include/packing.hlsli"

ROOT_CONSTANTS(ComposeRootConstants, rc)

float3 CheapACES(float3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}


float TonemapUchimura(float x, float P, float a, float m, float l, float c, float b) {
    // Uchimura 2017, "HDR theory and practice"
    // Math: https://www.desmos.com/calculator/gslcdxvipg
    // Source: https://www.slideshare.net/nikuque/hdr-theory-and-practicce-jp
    float l0 = ((P - m) * l) / a;
    float L0 = m - m / a;
    float L1 = m + (1.0 - m) / a;
    float S0 = m + l0;
    float S1 = m + a * l0;
    float C2 = (a * P) / (P - S1);
    float CP = -C2 / P;

    float w0 = 1.0 - smoothstep(0.0, m, x);
    float w2 = step(m + l0, x);
    float w1 = 1.0 - w0 - w2;

    float T = m * pow(x / m, c) + b;
    float S = P - (P - S1) * exp(CP * (x - S0));
    float L = m + a * (x - m);

    return T * w0 + L * w1 + S * w2;
}

float TonemapUchimura(float x) {
    const float P = 1.0;  // max display brightness
    const float a = 1.0;  // contrast
    const float m = 0.22; // linear section start
    const float l = 0.4;  // linear section length
    const float c = 1.1; // black
    const float b = 0.0;  // pedestal
    return TonemapUchimura(x, P, a, m, l, c, b);
}

float3 CheapChromaticAberration(Texture2D inTexture, float2 inUV)
{
    uint mip, width, height, levels;
    inTexture.GetDimensions(mip, width, height, levels);
    float separation_factor = (1.0 / width) * rc.mChromaticAberrationStrength;
	
    float3 color;
    
    color.r = inTexture.Sample(SamplerLinearClamp, float2(inUV.x + separation_factor, inUV.y)).r;
    color.g = inTexture.Sample(SamplerLinearClamp, inUV).g;
    color.b = inTexture.Sample(SamplerLinearClamp, float2(inUV.x - separation_factor, inUV.y)).b;
    
    color *= (1.0 - separation_factor * 0.5);
	
    return color;
}

float3 EncodeGamma(float3 L) {
    return pow(L, (1.0 / 2.2).xxx);
}

float4 main(in FULLSCREEN_TRIANGLE_VS_OUT inParams) : SV_Target0 {
    Texture2D input_texture = ResourceDescriptorHeap[rc.mInputTexture];
    Texture2D bloom_texture = ResourceDescriptorHeap[rc.mBloomTexture];
    
    float4 src = float4(CheapChromaticAberration(input_texture, inParams.mScreenUV), 1.0);
    
    src = lerp(src, bloom_texture.SampleLevel(SamplerLinearClamp, inParams.mScreenUV, 0), rc.mBloomBlendFactor);

    src.r = TonemapUchimura(src.r);
    src.g = TonemapUchimura(src.g);
    src.b = TonemapUchimura(src.b);
   
    return float4(EncodeGamma(src.rgb * rc.mExposure), 1.0);
}
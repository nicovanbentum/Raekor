#include "Include/Bindless.hlsli"
#include "Include/Common.hlsli"
#include "Include/Packing.hlsli"

ROOT_CONSTANTS(ComposeRootConstants, rc)

float3 ApplyCheapACES(float3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}


float ApplyTonemapUchimura(float x, float P, float a, float m, float l, float c, float b) {
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

float ApplyTonemapUchimura(float x) {
    const float P = 1.0;  // max display brightness
    const float a = 1.0;  // contrast
    const float m = 0.22; // linear section start
    const float l = 0.4;  // linear section length
    const float c = 1.1; // black
    const float b = 0.0;  // pedestal
    return ApplyTonemapUchimura(x, P, a, m, l, c, b);
}

float3 ApplyTonemapUchimura(float3 inColor)
{
    return float3(
        ApplyTonemapUchimura(inColor.x),
        ApplyTonemapUchimura(inColor.y),
        ApplyTonemapUchimura(inColor.z)
    );
}

float3 PBRNeutralToneMapping( float3 color ) {
  const float startCompression = 0.8 - 0.04;
  const float desaturation = 0.15;

  float x = min(color.r, min(color.g, color.b));
  float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
  color -= offset;

  float peak = max(color.r, max(color.g, color.b));
  if (peak < startCompression) return color;

  const float d = 1. - startCompression;
  float newPeak = 1. - d * d / (peak + d - startCompression);
  color *= newPeak / peak;

  float g = 1. - 1. / (desaturation * (peak - newPeak) + 1.);
  return lerp(color, newPeak * float3(1, 1, 1), g);
}

float3 ApplyChromaticAberration(Texture2D inTexture, float2 inUV)
{
    uint mip, width, height, levels;
    inTexture.GetDimensions(mip, width, height, levels);
    float separation_factor = (1.0 / width) * rc.mSettings.mChromaticAberrationStrength;
	
    float3 color;
    
    color.r = inTexture.Sample(SamplerLinearClamp, float2(inUV.x + separation_factor, inUV.y)).r;
    color.g = inTexture.Sample(SamplerLinearClamp, inUV).g;
    color.b = inTexture.Sample(SamplerLinearClamp, float2(inUV.x - separation_factor, inUV.y)).b;
    
    color *= (1.0 - separation_factor * 0.5);
	
    return color;
}

float3 ApplyBloom(float3 inColor, float2 inUV)
{
    Texture2D bloom_texture = ResourceDescriptorHeap[rc.mBloomTexture];
    float4 bloom = bloom_texture.SampleLevel(SamplerLinearClamp, inUV, 0);

    return lerp(inColor, bloom.rgb, rc.mSettings.mBloomBlendFactor);
}

float3 ApplyVignette(float3 inColor, float2 inUV)
{
    float r = length(inUV * 2.0 - 1.0);
    
    r = r * rc.mSettings.mVignetteScale - rc.mSettings.mVignetteBias;
    r = r * smoothstep(rc.mSettings.mVignetteInner, rc.mSettings.mVignetteOuter, r);
    
    return inColor * (1.0 - r);
}

float3 ApplyGammaCurve(float3 L) {
    return pow(L, (1.0 / 2.2).xxx);
}


float4 main(in FULLSCREEN_TRIANGLE_VS_OUT inParams) : SV_Target0 {
    Texture2D input_texture = ResourceDescriptorHeap[rc.mInputTexture];
    
    float3 color = ApplyChromaticAberration(input_texture, inParams.mScreenUV);
    
    color = ApplyBloom(color, inParams.mScreenUV);

    color = ApplyTonemapUchimura(color);

    color = ApplyGammaCurve(color);

    color = ApplyVignette(color, inParams.mScreenUV);
   
    return float4(color, 1.0);
}
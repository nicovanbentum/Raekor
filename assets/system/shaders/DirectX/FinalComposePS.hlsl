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
    
    src = lerp(src, bloom_texture.SampleLevel(SamplerLinearClamp, inParams.mScreenUV, 0), 0.06);
    
    return float4(EncodeGamma(src.rgb * rc.mExposure), 1.0);
}
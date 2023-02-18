#include "include/bindless.hlsli"
#include "include/common.hlsli"
#include "include/packing.hlsli"

struct RootConstants {
    uint mInputTexture;
};

ROOT_CONSTANTS(RootConstants, rc)

float3 CorrectGamma(float3 L) {
    return pow(L, (1.0 / 2.2).xxx);
}

float4 main(in FULLSCREEN_TRIANGLE_VS_OUT inParams) : SV_Target0 {
    Texture2D input_texture = ResourceDescriptorHeap[rc.mInputTexture];
    float4 src = input_texture.SampleLevel(SamplerLinearClamp, inParams.mScreenUV, 0);
    
    return float4(CorrectGamma(src.rgb), 1.0);
}
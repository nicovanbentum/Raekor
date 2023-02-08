#include "include/common.hlsli"
#include "include/packing.hlsli"

struct VS_OUTPUT {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

struct RootConstants {
    uint mInputTexture;
};

ROOT_CONSTANTS(RootConstants, rc)

float3 tonemap(float3 L) {
    return pow(L, (1.0 / 2.2).xxx);
}

float4 main(in VS_OUTPUT input) : SV_Target0 {
    Texture2D input_texture = ResourceDescriptorHeap[rc.mInputTexture];
    float4 src = input_texture.SampleLevel(SamplerPointClamp, input.uv, 0);
    
    
    return float4(tonemap(src.rgb), 1.0);
}
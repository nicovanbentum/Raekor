#include "include/common.hlsli"
#include "include/packing.hlsli"

struct VS_OUTPUT {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

struct RootConstants {
    uint mInputTexture;
};

ROOT_CONSTANTS(RootConstants, root_constants)

float4 main(in VS_OUTPUT input) : SV_Target0 {
    Texture2D<float4> input_texture = ResourceDescriptorHeap[root_constants.mInputTexture];
    float4 src = input_texture.SampleLevel(SamplerPointClamp, input.uv, 0);
    return float4(src.rgb, 1.0);
}
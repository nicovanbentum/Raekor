#include "include/bindless.hlsli"

struct VS_OUTPUT
{
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
};

ROOT_CONSTANTS(ImGuiRootConstants, rc)


float4 main(VS_OUTPUT input) : SV_TARGET {
    float4 sampled = float4(1.0, 1.0, 1.0, 1.0);

    Texture2D texture = ResourceDescriptorHeap[rc.mBindlessTextureIndex];
    sampled = texture.Sample(SamplerLinearClamp, input.uv);

    sampled.a = smoothstep(0.0, 1.0, sampled.r);
    sampled = float4(1, 1, 1, sampled.a);

    return input.color * sampled;
}
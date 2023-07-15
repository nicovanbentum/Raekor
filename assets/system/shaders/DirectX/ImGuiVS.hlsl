#include "include/bindless.hlsli"

struct VS_INPUT
{
    float2 pos : POSITION;
    float2 uv : TEXCOORD;
    uint color : COLOR;
};

struct VS_OUTPUT
{
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
};

ROOT_CONSTANTS(ImGuiRootConstants, rc)

VS_OUTPUT main(in VS_INPUT input, out float4 pos : SV_Position) {
    pos = mul(rc.mProjection, float4(input.pos, 0.0, 1.0));
    pos.z = (pos.z / pos.w * 0.5 + 0.5) * pos.w;

    VS_OUTPUT output;
    output.uv = input.uv;

    output.color.w = float((input.color & uint(0xff000000)) >> 24) / 255;
	output.color.z = float((input.color & uint(0x00ff0000)) >> 16) / 255;
	output.color.y = float((input.color & uint(0x0000ff00)) >> 8) / 255;
	output.color.x = float((input.color & uint(0x000000ff)) >> 0) / 255;

    return output;
}


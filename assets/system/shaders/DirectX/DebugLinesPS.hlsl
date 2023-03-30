#include "include/bindless.hlsli"
#include "include/packing.hlsli"

struct VS_OUTPUT
{
    float4 mWorldPos : SV_Position;
    float4 mColor : COLOR0;
};

float4 main(in VS_OUTPUT inParams) : SV_Target0
{
    return inParams.mColor;
}
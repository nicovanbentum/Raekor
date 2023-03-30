#include "include/bindless.hlsli"
#include "include/packing.hlsli"

struct VS_OUTPUT
{
    float4 mWorldPos : SV_Position;
    float4 mColor : COLOR0;
};


VS_OUTPUT main(in float4 inVertex : POSITION)
{
    VS_OUTPUT output;
    
    FrameConstants fc = gGetFrameConstants();
    
    output.mWorldPos = mul(fc.mViewProjectionMatrix, float4(inVertex.xyz, 1.0));
    output.mColor = RGBA8ToFloat4(inVertex.w);
    
    return output;
}

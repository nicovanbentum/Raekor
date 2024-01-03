#include "include/bindless.hlsli"
#include "include/packing.hlsli"

struct VS_OUTPUT
{
    float4 mWorldPos : SV_Position;
    float4 mColor : COLOR0;
};

ROOT_CONSTANTS(DebugPrimitivesRootConstants, rc)

VS_OUTPUT main(
#ifdef DEBUG_PROBE_RAYS
    in float4 inVertex : POSITION, 
#endif
in uint inVertexID : SV_VertexID
)
{
    VS_OUTPUT output;
    
    FrameConstants fc = gGetFrameConstants();

#ifndef DEBUG_PROBE_RAYS
    float4 inVertex = PassConstantsBuffer.Load<float4>(rc.mBufferOffset + sizeof(float4) * inVertexID);
#endif
    
    output.mWorldPos = mul(fc.mViewProjectionMatrix, float4(inVertex.xyz, 1.0));
    output.mColor = RGBA8ToFloat4(asuint(inVertex.w));
    
    return output;
}

#include "Include/Bindless.hlsli"
#include "Include/Packing.hlsli"

struct VS_OUTPUT
{
    float4 mWorldPos : SV_Position;
    float4 mColor : COLOR0;
};

PASS_CONSTANTS(pc)
FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(DebugPrimitivesRootConstants, rc)

VS_OUTPUT main(in uint inVertexID : SV_VertexID)
{
    VS_OUTPUT output;

#ifdef DEBUG_PROBE_RAYS
    RWStructuredBuffer<float4> vertex_buffer = ResourceDescriptorHeap[fc.mDebugLinesVertexBuffer];
    float4 inVertex = vertex_buffer[inVertexID];
#else 
    float4 inVertex = pc.Load<float4>(rc.mBufferOffset + sizeof(float4) * inVertexID);
#endif
    
    output.mWorldPos = mul(fc.mViewProjectionMatrix, float4(inVertex.xyz, 1.0));
    output.mColor = RGBA8ToFloat4(asuint(inVertex.w));
    
    return output;
}

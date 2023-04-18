#include "include/bindless.hlsli"

struct VS_OUTPUT {
    float4 sv_position      : SV_Position;
    float4 curr_position    : POS0;
    float4 prev_position    : POS1;
    float2 texcoord         : TEXCOORD;
    float3 normal           : NORMAL;
    float3 tangent          : TANGENT;
    float3 bitangent        : BINORMAL;
};


ROOT_CONSTANTS(GbufferRootConstants, rc)

struct Transform {
    float4x4 mat;
};

VS_OUTPUT main(in uint inVertexID : SV_VertexID)
{
    FrameConstants fc = gGetFrameConstants();
    
    StructuredBuffer<RTVertex> vertex_buffer = ResourceDescriptorHeap[rc.mVertexBuffer];
    
    RTVertex vertex = vertex_buffer[inVertexID];

    VS_OUTPUT output;
    output.normal = normalize(vertex.mNormal);
    output.tangent = normalize(vertex.mTangent);
    output.tangent = normalize(output.tangent - dot(output.tangent, output.normal) * output.normal);
    output.bitangent = normalize(cross(output.normal, output.tangent));
    output.texcoord = vertex.mTexCoord;
    
    output.curr_position = mul(fc.mViewProjectionMatrix, float4(vertex.mPos, 1.0));
    output.prev_position = mul(fc.mPrevViewProjectionMatrix, float4(vertex.mPos, 1.0));
    output.sv_position = output.curr_position;
    return output;
}

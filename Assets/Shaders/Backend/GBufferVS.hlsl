#include "Include/Common.hlsli"

struct VS_OUTPUT {
    float4 sv_position      : SV_Position;
    float4 curr_position    : POS0;
    float4 prev_position    : POS1;
    float2 texcoord         : TEXCOORD;
    float3 normal           : NORMAL;
    float3 tangent          : TANGENT;
    float3 bitangent        : BINORMAL;
    nointerpolation uint instance_index : INSTANCE;
};

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(GbufferRootConstants, rc)

VS_OUTPUT main(in uint inVertexID : SV_VertexID, in uint inInstanceID : SV_InstanceID)
{
    const uint instance_index = rc.mInstanceIndex + inInstanceID;

    StructuredBuffer<RTGeometry> geometries = ResourceDescriptorHeap[fc.mInstancesBuffer];
    RTGeometry geometry = geometries[instance_index];
    
    StructuredBuffer<RTVertex> vertex_buffer = ResourceDescriptorHeap[geometry.mVertexBuffer];

    RTVertex vertex = vertex_buffer[inVertexID];
    float4 prev_pos_ws = mul(geometry.mPrevWorldTransform, float4(vertex.mPos, 1.0));
    
    TransformToWorldSpace(vertex, geometry.mWorldTransform);
    float4 curr_pos_ws = float4(vertex.mPos, 1.0);

    VS_OUTPUT output;
    output.normal = normalize(vertex.mNormal);
    output.tangent = normalize(vertex.mTangent);
    output.tangent = normalize(output.tangent - dot(output.tangent, output.normal) * output.normal);
    output.bitangent = normalize(cross(output.normal, output.tangent));
    output.texcoord = vertex.mTexCoord;
    output.instance_index = instance_index;

    // TODO: prev world transform
    output.curr_position = mul(fc.mViewProjectionMatrix, curr_pos_ws);
    output.prev_position = mul(fc.mPrevViewProjectionMatrix, prev_pos_ws);
    output.sv_position = output.curr_position;
    return output;
}


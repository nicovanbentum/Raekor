#include "Include/Common.hlsli"

struct VS_OUTPUT 
{
    float4 position      : SV_Position;
};

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(ShadowMapRootConstants, rc)

VS_OUTPUT main(in uint inVertexID : SV_VertexID)
{
    StructuredBuffer<RTGeometry> geometries = ResourceDescriptorHeap[fc.mInstancesBuffer];
    RTGeometry geometry = geometries[rc.mInstanceIndex];
    
    StructuredBuffer<RTVertex> vertex_buffer = ResourceDescriptorHeap[geometry.mVertexBuffer];
    RTVertex vertex = vertex_buffer[inVertexID];
    
    TransformToWorldSpace(vertex, geometry.mWorldTransform);

    VS_OUTPUT output;

    output.position = mul(rc.mViewProjMatrix, float4(vertex.mPos, 1.0));
    
    return output;
}


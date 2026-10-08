#include "Include/Bindless.hlsli"
#include "Include/Common.hlsli"

struct VS_OUTPUT
{
    float4 mPosition : SV_Position;
    float4 mColor : COLOR0;
};

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(WireframeRootConstants, rc)

VS_OUTPUT main(in uint inVertexID : SV_VertexID)
{
    StructuredBuffer<RTGeometry> geometries = ResourceDescriptorHeap[fc.mInstancesBuffer];
    RTGeometry geometry = geometries[rc.mInstanceIndex];

    StructuredBuffer<RTVertex> vertex_buffer = ResourceDescriptorHeap[geometry.mVertexBuffer];

    RTVertex vertex = vertex_buffer[inVertexID];
    TransformToWorldSpace(vertex, geometry.mWorldTransform);

    float4 position = mul(fc.mViewProjectionMatrix, float4(vertex.mPos, 1.0));
    position.xy -= fc.mJitter * position.w;

    VS_OUTPUT output;
    output.mPosition = position;
    output.mColor = float4(rc.mColor & 0xFF, (rc.mColor >> 8) & 0xFF, (rc.mColor >> 16) & 0xFF, (rc.mColor >> 24) & 0xFF) / 255.0f;
    return output;
}

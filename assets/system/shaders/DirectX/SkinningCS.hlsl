#include "include/bindless.hlsli"
#include "include/common.hlsli"

ROOT_CONSTANTS(SkinningRootConstants, rc)

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID, uint groupIndex : SV_GroupIndex)
{
    StructuredBuffer<int4> bone_indices_buffer = ResourceDescriptorHeap[rc.mBoneIndicesBuffer];
    StructuredBuffer<float4> bone_weights_buffer = ResourceDescriptorHeap[rc.mBoneWeightsBuffer];
    StructuredBuffer<float4x4> bone_transforms_buffer = ResourceDescriptorHeap[rc.mBoneTransformsBuffer];

    StructuredBuffer<RTVertex> vertex_buffer = ResourceDescriptorHeap[rc.mMeshVertexBuffer];
    RWStructuredBuffer<RTVertex> skinned_vertex_buffer = ResourceDescriptorHeap[rc.mSkinnedVertexBuffer];

    uint vertex_id = dispatchThreadID.x;
    if ( vertex_id > rc.mDispatchSize )
        return;

    int4 bone = bone_indices_buffer[vertex_id];
    float4 weight = bone_weights_buffer[vertex_id];

    float4x4 bone_transform  = bone_transforms_buffer[bone[0]] * weight[0];
             bone_transform += bone_transforms_buffer[bone[1]] * weight[1];
             bone_transform += bone_transforms_buffer[bone[2]] * weight[2];
             bone_transform += bone_transforms_buffer[bone[3]] * weight[3];

    RTVertex vertex = vertex_buffer[vertex_id];
    vertex.mPos = mul(bone_transform, float4(vertex.mPos, 1.0)).xyz;
    vertex.mNormal = mul(bone_transform, float4(vertex.mNormal, 0.0)).xyz;
    vertex.mTangent = mul(bone_transform, float4(vertex.mTangent, 0.0)).xyz;
    
    skinned_vertex_buffer[vertex_id] = vertex;
}

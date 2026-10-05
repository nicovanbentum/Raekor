#include "Include/Bindless.hlsli"
#include "Include/DepthOfField.hlsli"

ROOT_CONSTANTS(DepthOfFieldRootConstants, rc)

groupshared float lds_MaxCoC[DOF_TILE_SIZE * DOF_TILE_SIZE];

[numthreads(DOF_TILE_SIZE, DOF_TILE_SIZE, 1)]
void main(uint2 threadID : SV_DispatchThreadID, uint2 groupID : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    Texture2D<float4> half_res_texture = ResourceDescriptorHeap[rc.mHalfResTexture];
    RWTexture2D<float> tile_texture = ResourceDescriptorHeap[rc.mTileTexture];

    uint2 half_size;
    half_res_texture.GetDimensions(half_size.x, half_size.y);

    lds_MaxCoC[groupIndex] = all(threadID < half_size) ? abs(half_res_texture[threadID].a) : 0.0f;

    GroupMemoryBarrierWithGroupSync();

    for (uint stride = (DOF_TILE_SIZE * DOF_TILE_SIZE) / 2; stride > 0; stride >>= 1)
    {
        if (groupIndex < stride)
            lds_MaxCoC[groupIndex] = max(lds_MaxCoC[groupIndex], lds_MaxCoC[groupIndex + stride]);

        GroupMemoryBarrierWithGroupSync();
    }

    if (groupIndex == 0)
        tile_texture[groupID] = lds_MaxCoC[0];
}

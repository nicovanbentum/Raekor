#include "include/bindless.hlsli"
#include "include/common.hlsli"

ROOT_CONSTANTS(ShadowsClassifyRootConstants, rc)

[numthreads(RT_SHADOWS_GROUP_DIM, RT_SHADOWS_GROUP_DIM, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID, uint groupIndex : SV_GroupIndex)
{
    Texture2D<uint> shadow_ray_texture = ResourceDescriptorHeap[rc.mShadowMaskTexture];
    RWStructuredBuffer<uint> tiles_buffer = ResourceDescriptorHeap[rc.mTilesBuffer];
    RWByteAddressBuffer dispatch_buffer = ResourceDescriptorHeap[rc.mDispatchBuffer];
    
    //if (any(dispatchThreadID.xy >= rc.mDispatchSize))
    //    return;
    
    // every group writes its coordinates to the tile buffer
    if (groupIndex == 0)
    {
        // add 1 to the X dispatch thread count and mark the tile as needing denoising
        uint tile_index;
        dispatch_buffer.InterlockedAdd(0 * sizeof(uint), 1, tile_index);
        tiles_buffer[tile_index] = UInt2ToUInt(groupID.x, groupID.y);
        
        // set the Y and Z dispatch thread count to 1
        uint temp_value;
        dispatch_buffer.InterlockedExchange(1 * sizeof(uint), 1, temp_value);
        dispatch_buffer.InterlockedExchange(2 * sizeof(uint), 1, temp_value);
    }
}

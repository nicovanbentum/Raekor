#include "include/bindless.hlsli"
#include "include/common.hlsli"

ROOT_CONSTANTS(ShadowsDenoiseRootConstants, rc)

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID)
{
    StructuredBuffer<uint> tiles_buffer = ResourceDescriptorHeap[rc.mTilesBuffer];
    RWTexture2D<float2> result_texture = ResourceDescriptorHeap[rc.mResultTexture];
    Texture2D<uint> ray_hits_texture = ResourceDescriptorHeap[rc.mShadowMaskTexture];
    
    uint tile_index = groupID.x;
    uint2 group_coord = UIntToUInt2(tiles_buffer[tile_index]);
    uint2 pixel_coord = group_coord * 16 + groupThreadID.xy;
   
    uint2 ray_hit_coords = uint2(pixel_coord.x / 8, pixel_coord.y / 4);
    uint ray_hits = ray_hits_texture[ray_hit_coords];
    
    uint2 mask_coords = uint2(pixel_coord.x % 8, pixel_coord.y % 4);
    uint hit = ray_hits & (1u << (mask_coords.x * mask_coords.y));
    
    result_texture[pixel_coord] = ray_hits ? 1.0f : 0.0f;
}
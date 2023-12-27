#include "include/bindless.hlsli"
#include "include/common.hlsli"

ROOT_CONSTANTS(ShadowsDenoiseRootConstants, rc)

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID)
{
    StructuredBuffer<uint> tiles_buffer = ResourceDescriptorHeap[rc.mTilesBuffer];
    Texture2D<float2> history_texture = ResourceDescriptorHeap[rc.mHistoryTexture];
    RWTexture2D<float2> result_texture = ResourceDescriptorHeap[rc.mResultTexture];
    Texture2D<uint> ray_hits_texture = ResourceDescriptorHeap[rc.mShadowMaskTexture];
    Texture2D<float2> velocity_texture = ResourceDescriptorHeap[rc.mVelocityTexture];
    
    FrameConstants fc = gGetFrameConstants();
    
    uint tile_index = groupID.x;
    uint2 group_coord = UIntToUInt2(tiles_buffer[tile_index]);
    uint2 pixel_coord = group_coord * 16 + groupThreadID.xy;
    float2 screen_uv = float2(pixel_coord + 0.5) / rc.mDispatchSize;
    
    uint2 ray_hit_coords = uint2(pixel_coord.x / 8, pixel_coord.y / 4);
    uint ray_hits = ray_hits_texture[ray_hit_coords];
    
    uint2 group_thread_id = uint2(pixel_coord.x % 8, pixel_coord.y % 4);
    uint group_index = group_thread_id.y * 8 + group_thread_id.x;
    uint hit = ray_hits & (1u << group_index);
 
#if 0
    if (fc.mFrameCounter > 0)
    {
        float2 velocity = velocity_texture[pixel_coord].xy;
        float2 history_uv = saturate(screen_uv - velocity);
        
        float2 history_sample = history_texture.Sample(SamplerPointClamp, screen_uv).rg;
    
        float history_length = history_sample.y + 1.0f;
        
        float accumulated_sample = lerp(history_sample.r, hit ? 0.0f : 1.0f, 0.01);
    
        result_texture[pixel_coord] = float2(accumulated_sample, history_length);
    }
    else
#endif
    {
        result_texture[pixel_coord] = float2(hit ? 0.0f : 1.0f, 1.0f);
    }
}
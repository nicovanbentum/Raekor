#include "Include/Bindless.hlsli"
#include "Include/Common.hlsli"
#include "Include/Material.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(ShadowsDenoiseRootConstants, rc)

#define NORMAL_DISTANCE 0.1f
#define PLANE_DISTANCE 5.0f

bool ArePlanesValid(float3 inPos, float3 inPrevPos, float3 inNormal)
{
    float3 to_current = inPos - inPrevPos;
    float dist_to_plane = abs(dot(to_current, inNormal));
    return dist_to_plane <= PLANE_DISTANCE;
}

bool AreNormalsValid(float3 inNormal, float3 inPrevNormal)
{
    return (pow(abs(dot(inNormal, inPrevNormal)), 2) <= NORMAL_DISTANCE);
}

bool AreEntitiesValid(uint inEntity, uint inPrevEntity)
{
    return inEntity == inPrevEntity;
}

bool IsReprojectionValid(float3 inPos, float3 inPrevPos, float3 inNormal, float3 inPrevNormal, uint inEntity, uint inPrevEntity)
{
    if (!AreEntitiesValid(inEntity, inPrevEntity))
        return false;
    
    if (!ArePlanesValid(inPos, inPrevPos, inNormal))
        return false;
    
    //if (!AreNormalsValid(inNormal, inPrevNormal))
    //    return false;
    
    return true;
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID)
{
    StructuredBuffer<uint> tiles_buffer = ResourceDescriptorHeap[rc.mTilesBuffer];
    Texture2D<float2> history_texture = ResourceDescriptorHeap[rc.mHistoryTexture];
    RWTexture2D<float2> result_texture = ResourceDescriptorHeap[rc.mResultTexture];
    Texture2D<uint> ray_hits_texture = ResourceDescriptorHeap[rc.mShadowMaskTexture];
    
    Texture2D<float>   depth_texture     = ResourceDescriptorHeap[rc.mDepthTexture];
    Texture2D<float4>  gbuffer_texture   = ResourceDescriptorHeap[rc.mGBufferTexture];
    Texture2D<float2>  velocity_texture  = ResourceDescriptorHeap[rc.mVelocityTexture];
    Texture2D<uint>    selection_texture = ResourceDescriptorHeap[rc.mSelectionTexture];
    
    uint tile_index = groupID.x;
    uint2 group_coord = UIntToUInt2(tiles_buffer[tile_index]);
    uint2 pixel_coord = group_coord * 16 + groupThreadID.xy;
    
    float2 velocity = velocity_texture[pixel_coord].xy;
    float2 screen_uv = float2(pixel_coord + 0.5) / rc.mDispatchSize;
    
    float2 prev_screen_uv = saturate(screen_uv - velocity);
    uint2 prev_pixel_coord = floor(prev_screen_uv * fc.mViewportSize);
    
    uint2 ray_hit_coords = uint2(pixel_coord.x / 8, pixel_coord.y / 4);
    uint ray_hits = ray_hits_texture[ray_hit_coords];
    
    uint2 group_thread_id = uint2(pixel_coord.x % 8, pixel_coord.y % 4);
    uint group_index = group_thread_id.y * 8 + group_thread_id.x;
    uint hit = ray_hits & (1u << group_index);
    
    uint entity = selection_texture[pixel_coord];
    uint prev_entity = selection_texture[prev_pixel_coord];
    
    float4 gbuffer = gbuffer_texture[pixel_coord];
    float4 prev_gbuffer = gbuffer_texture[prev_pixel_coord];
    
    BRDF gbuffer_brdf, prev_gbuffer_brdf;
    gbuffer_brdf.Unpack(asuint(gbuffer));
    prev_gbuffer_brdf.Unpack(asuint(prev_gbuffer));
    
    float depth = depth_texture[pixel_coord];
    float prev_depth = depth_texture[prev_pixel_coord];
    
    float3 position = ReconstructWorldPosition(screen_uv, depth, fc.mInvViewProjectionMatrix);
    float3 prev_position = ReconstructWorldPosition(prev_screen_uv, prev_depth, fc.mPrevViewProjectionMatrix);
    
    bool should_reproject = IsReprojectionValid(position, prev_position, gbuffer_brdf.mNormal, prev_gbuffer_brdf.mNormal, entity, prev_entity);
    
    if (fc.mFrameCounter == 0)
        should_reproject = false;
    
#if 0
    if (should_reproject)
    {
        float2 history_sample = history_texture.SampleLevel(SamplerPointClamp, prev_screen_uv, 0).rg;
        //float2 history_sample = history_texture[pixel_coord];
        float history_length = history_sample.y + 1.0f;
        
        float accumulated_sample = lerp(history_sample.r, hit ? 0.0f : 1.0f, 0.1);
    
        result_texture[pixel_coord] = float2(accumulated_sample.x, history_length);
    }
    else
#endif
    {
        result_texture[pixel_coord] = float2(hit ? 0.0f : 1.0f, 1.0f);
    }
}
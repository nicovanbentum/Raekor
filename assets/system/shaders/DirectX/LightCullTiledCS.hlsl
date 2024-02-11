#include "include/bindless.hlsli"
#include "include/common.hlsli"

bool IsLightCulled(RTLight inLight);

ROOT_CONSTANTS(TiledLightCullingRootConstants, rc)

groupshared uint g_LightCount;
groupshared uint g_LightIndices[LIGHT_CULL_MAX_LIGHTS];

[numthreads(LIGHT_CULL_TILE_SIZE, LIGHT_CULL_TILE_SIZE, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID, uint groupIndex : SV_GroupIndex)
{
    RWByteAddressBuffer light_count_buffer = ResourceDescriptorHeap[rc.mLightGridBuffer];
    RWByteAddressBuffer light_indices_buffer = ResourceDescriptorHeap[rc.mLightIndicesBuffer];
    RWStructuredBuffer<RTLight> bindless_lights_buffer = ResourceDescriptorHeap[rc.mLightsBuffer];
 
    if (any(dispatchThreadID.xy >= rc.mFullResSize))
        return;

    if (groupIndex == 0)
        g_LightCount = 0;

    GroupMemoryBarrierWithGroupSync();

    for (int light_idx = 0; light_idx < rc.mLightsCount; light_idx++)
    {
        RTLight light = bindless_lights_buffer[light_idx];

        bool culled = IsLightCulled(light);

        if (!culled)
        {
            uint lds_light_index;
            InterlockedAdd(g_LightCount, 1, lds_light_index);

            if (lds_light_index < LIGHT_CULL_MAX_LIGHTS)
                g_LightIndices[lds_light_index] = light_idx;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (groupIndex == 0)
    {
        uint light_count = min(g_LightCount, LIGHT_CULL_MAX_LIGHTS);
        uint count_offset = rc.mDispatchSize.x * groupID.y + groupID.x;
        light_count_buffer.Store(count_offset, light_count);
        
        uint indices_offset = rc.mDispatchSize.x * LIGHT_CULL_MAX_LIGHTS * groupID.y + groupID.x * LIGHT_CULL_MAX_LIGHTS;
        
        for (int i = 0; i < light_count; i++)
            light_indices_buffer.Store(indices_offset + i, g_LightIndices[i]);
    }
}


bool IsLightCulled(RTLight inLight) 
{ 
    return false; 
}


//void GetLightIndices(uint2 inPixelCoords, TiledLightCullingRootConstants inConstants, out uint outOffset, out uint outCount)
//{
//    uint2 group_index = inPixelCoords / LIGHT_CULL_TILE_SIZE;
//    RWByteAddressBuffer light_count_buffer = ResourceDescriptorHeap[inConstants.mLightGridBuffer];
//    
//    outCount = light_count_buffer.Load(inConstants.mTileCount.x.x * group_index.x + group_index.y);
//    outOffset = inConstants.mTileCount.x * group_index.x * LIGHT_CULL_MAX_LIGHTS + group_index.y;
//}

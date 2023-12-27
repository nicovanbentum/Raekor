#include "include/bindless.hlsli"
#include "include/common.hlsli"

ROOT_CONSTANTS(ShadowsClearRootConstants, rc)

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID, uint groupIndex : SV_GroupIndex)
{
    RWStructuredBuffer<uint> tiles_buffer = ResourceDescriptorHeap[rc.mTilesBuffer];
    RWByteAddressBuffer dispatch_buffer = ResourceDescriptorHeap[rc.mDispatchBuffer];
    
    tiles_buffer[dispatchThreadID.x] = 0;
    
    if (dispatchThreadID.x == 0)
        dispatch_buffer.Store4(0, 0.xxxx);
}

#include "Include/Bindless.hlsli"
#include "Include/Packing.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"
#include "Include/DDGI.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(ProbeUpdateRootConstants, rc)

[numthreads(64, 1, 1)]
void main(uint3 threadID : SV_DispatchThreadID,  uint3 groupThreadID : SV_GroupThreadID, uint3 groupID : SV_GroupID, uint inGroupIndex : SV_GroupIndex)
{
    Texture2D<float> rays_depth_texture = ResourceDescriptorHeap[rc.mDDGIData.mRaysDepthTexture];
    RWStructuredBuffer<ProbeData> probe_buffer = ResourceDescriptorHeap[rc.mDDGIData.mProbesDataBuffer];
    
    // 1D index of the probe we are on, used to read the 192 ray hits from the ray tracing results
    uint probe_index = threadID.x;
    
    // calculate how many rays the current thread should write to lds
    const uint rays_per_lane = DDGI_RAYS_PER_PROBE / 64;
    
#if 0
    for (uint i = 0; i < rays_per_lane; i++)
    {
        uint ray_index = inGroupIndex * rays_per_lane + i;
        lds_ProbeDepthRays[ray_index] = rays_depth_texture[uint2(ray_index, probe_index)];
    }
    
    GroupMemoryBarrierWithGroupSync();
#endif

    ProbeData probe_data;
    probe_data.offset = 0.xxx;
    probe_data.inactive = false;
    
    uint backface_count = 0;
    for (uint ray_index = 0; ray_index < DDGI_RAYS_PER_PROBE; ray_index++)
    {
        float depth = rays_depth_texture[uint2(ray_index, probe_index)];
        
        if (depth < 0.0f)
        {
            if (++backface_count >= DDGI_RAYS_BACKFACE_THRESHOLD)
            {
                probe_data.inactive = true;
                break;
            }
        }
    }
    
    probe_buffer[probe_index] = probe_data;
}
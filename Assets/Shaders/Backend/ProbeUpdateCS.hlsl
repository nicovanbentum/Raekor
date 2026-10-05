#include "Include/Bindless.hlsli"
#include "Include/Packing.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"
#include "Include/DDGI.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(ProbeUpdateRootConstants, rc)

[numthreads(64, 1, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    Texture2D<float> rays_depth_texture = ResourceDescriptorHeap[rc.mDDGIData.mRaysDepthTexture];
    RWStructuredBuffer<ProbeData> probe_buffer = ResourceDescriptorHeap[rc.mDDGIData.mProbesDataBuffer];

    // 1D index of the probe we are on, used to read the 192 ray hits from the ray tracing results
    uint probe_index = threadID.x;

    if (probe_index >= DDGIGetProbesPerCascade(rc.mDDGIData) * rc.mDDGIData.mCascadeCount)
        return;

    DDGIVolume volume = DDGIGetVolume(DDGIGetProbeCascade(probe_index, rc.mDDGIData), rc.mDDGIData);
    int3 probe_cell = DDGIGetProbeCell(DDGIGetProbeGridCoord(probe_index, volume, rc.mDDGIData), volume);

    ProbeData probe_data = probe_buffer[probe_index];
    probe_data.reset = fc.mFrameCounter == 0 || any(probe_data.cell != probe_cell);

    if (probe_data.reset)
    {
        probe_data.offset = 0.xxx;
        probe_data.inactive = false;
        probe_data.cell = probe_cell;
    }

    uint backface_count = 0;

    int closest_backface_index = -1;
    int closest_frontface_index = -1;
    int farthest_frontface_index = -1;

    float closest_backface_distance = 1e27f;
    float closest_frontface_distance = 1e27f;
    float farthest_frontface_distance = 0.0f;

    for (uint ray_index = 0; ray_index < DDGI_RAYS_PER_PROBE; ray_index++)
    {
        float depth = rays_depth_texture[uint2(ray_index, probe_index)];

        if (depth < 0.0f)
        {
            backface_count++;

            depth = -depth * 5.0f;

            if (depth < closest_backface_distance)
            {
                closest_backface_distance = depth;
                closest_backface_index = ray_index;
            }
        }
        else
        {
            if (depth < closest_frontface_distance)
            {
                closest_frontface_distance = depth;
                closest_frontface_index = ray_index;
            }

            if (depth > farthest_frontface_distance)
            {
                farthest_frontface_distance = depth;
                farthest_frontface_index = ray_index;
            }
        }
    }

    const float min_spacing = DDGIGetMinProbeSpacing(volume);
    const float min_frontface_distance = 0.25f * min_spacing;

    float3 full_offset = 1e27f.xxx;

    if (closest_backface_index != -1 && float(backface_count) / DDGI_RAYS_PER_PROBE > 0.25f)
    {
        float3 closest_backface_dir = DDGIGetProbeRayDirection(closest_backface_index, rc.mRandomRotationMatrix);
        full_offset = probe_data.offset + closest_backface_dir * (closest_backface_distance + min_frontface_distance * 0.5f);
    }
    else if (closest_frontface_index != -1 && closest_frontface_distance < min_frontface_distance)
    {
        float3 closest_frontface_dir = DDGIGetProbeRayDirection(closest_frontface_index, rc.mRandomRotationMatrix);
        float3 farthest_frontface_dir = DDGIGetProbeRayDirection(farthest_frontface_index, rc.mRandomRotationMatrix);

        if (dot(closest_frontface_dir, farthest_frontface_dir) <= 0.0f)
            full_offset = probe_data.offset + farthest_frontface_dir * min(farthest_frontface_distance, min_frontface_distance);
    }
    else if (closest_frontface_distance > min_frontface_distance && dot(probe_data.offset, probe_data.offset) > 0.0f)
    {
        float offset_length = length(probe_data.offset);
        float move_back_distance = min(closest_frontface_distance - min_frontface_distance, offset_length);
        full_offset = probe_data.offset - (probe_data.offset / offset_length) * move_back_distance;
    }

    float3 normalized_offset = full_offset / volume.mProbeSpacing;

    if (all(abs(normalized_offset) <= 0.45f))
        probe_data.offset = full_offset;

    probe_data.inactive = backface_count >= DDGI_RAYS_BACKFACE_THRESHOLD;

    probe_buffer[probe_index] = probe_data;
}

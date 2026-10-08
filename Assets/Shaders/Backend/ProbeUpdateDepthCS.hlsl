#include "Include/Bindless.hlsli"
#include "Include/Packing.hlsli"
#include "Include/Common.hlsli"
#include "Include/Random.hlsli"
#include "Include/DDGI.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(ProbeUpdateRootConstants, rc)

groupshared float lds_ProbeDepthRays[DDGI_RAYS_PER_PROBE];
groupshared float3 lds_ProbeRayDirections[DDGI_RAYS_PER_PROBE];

[numthreads(DDGI_DEPTH_TEXELS, DDGI_DEPTH_TEXELS, 1)]
void main(uint3 threadID : SV_DispatchThreadID,  uint3 groupThreadID : SV_GroupThreadID, uint3 groupID : SV_GroupID, uint inGroupIndex : SV_GroupIndex)
{
    Texture2D<float> rays_depth_texture = ResourceDescriptorHeap[rc.mDDGIData.mRaysDepthTexture];
    RWTexture2D<float2> probes_depth_texture = ResourceDescriptorHeap[rc.mDDGIData.mProbesDepthTexture];
    RWStructuredBuffer<ProbeData> probe_buffer = ResourceDescriptorHeap[rc.mDDGIData.mProbesDataBuffer];

    // 1D index of the probe we are on, used to read the 192 ray hits from the ray tracing results
    uint probe_index = Index2DTo1D(groupID.xy, DDGI_PROBES_PER_ROW);

    if (probe_index >= DDGIGetProbesPerCascade(rc.mDDGIData) * rc.mDDGIData.mCascadeCount)
        return;

    ProbeData probe_data = probe_buffer[probe_index];

    const float max_depth = length(DDGIGetVolume(DDGIGetProbeCascade(probe_index, rc.mDDGIData), rc.mDDGIData).mProbeSpacing) * 1.5f;

    // calculate how many rays the current thread should write to lds
    const uint rays_per_lane = max(1u, DDGI_RAYS_PER_PROBE / (DDGI_DEPTH_TEXELS * DDGI_DEPTH_TEXELS));

    // unroll if possible as this number is usually quite small
    [unroll]
    for (uint i = 0; i < rays_per_lane; i++)
    {
        uint ray_index = inGroupIndex * rays_per_lane + i;

        if (ray_index < DDGI_RAYS_PER_PROBE)
        {
            // ray depth can be negative to indicate backface hit, so take abs
            lds_ProbeDepthRays[ray_index] = min(abs(rays_depth_texture[uint2(ray_index, probe_index)]), max_depth);
            lds_ProbeRayDirections[ray_index] = DDGIGetProbeRayDirection(ray_index, rc.mRandomRotationMatrix);
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // The 2D pixel coordinate on the probe's total texel area (with border)
    // every group is 1 probe, so get the 2d thread index within the group
    uint2 probe_pixel = groupThreadID.xy;

    bool is_border = probe_pixel.x == 0 || probe_pixel.x == (DDGI_DEPTH_TEXELS - 1) ||
                     probe_pixel.y == 0 || probe_pixel.y == (DDGI_DEPTH_TEXELS - 1);

    if (!is_border)
    {
        float2 octahedral_uv = ((float2(probe_pixel) + 0.5) / DDGI_DEPTH_TEXELS.xx) * 2.0 - 1.0;
        float3 octahedral_dir = OctDecode(octahedral_uv);

        float3 depth = 0.xxx;

        for (uint ray_index = DDGI_FIXED_RAYS; ray_index < DDGI_RAYS_PER_PROBE; ray_index++)
        {
            float ray_depth = lds_ProbeDepthRays[ray_index];
            float weight = pow(saturate(dot(octahedral_dir, lds_ProbeRayDirections[ray_index])), 50.0f);

            depth += float3(ray_depth * weight, ray_depth * ray_depth * weight, weight);
        }

        if (depth.z > 0.0)
            depth.rg /= depth.z;

        float2 prev_depth = probes_depth_texture[threadID.xy].rg;
        float2 final_depth = depth.rg;

        if (fc.mFrameCounter >= 2 && !probe_data.reset)
            final_depth = lerp(depth.rg, prev_depth, probe_data.inactive ? 1.0f : 0.97f);

        probes_depth_texture[threadID.xy] = final_depth;
    }

    AllMemoryBarrierWithGroupSync();

    if (is_border)
    {
        // Initialize the texel coordinateto copy to 0,0 in range [0 - nr_of_texels] (so basically the top left start of the probe texels)
        uint2 copy_texel = groupID.xy * DDGI_DEPTH_TEXELS;

        // Trick is here is to wrap around by using the absolute of signed integers
        // e.g. border at 0,0 maps to abs(0,0 - 1,1) which is 1,1 inner pixel,
        // using the same for 6,6 maps to abs(6,6 - 1,1) which is 5,5
        if (probe_pixel.y > 0 && probe_pixel.y < DDGI_DEPTH_TEXELS - 1)
        {
            copy_texel.x += abs(int(probe_pixel.x) - 1);
            copy_texel.y += DDGI_DEPTH_TEXELS - 1 - probe_pixel.y;
        }
        else if (probe_pixel.x > 0 && probe_pixel.x < DDGI_DEPTH_TEXELS - 1)
        {
            copy_texel.y += abs(int(probe_pixel.y) - 1);
            copy_texel.x += DDGI_DEPTH_TEXELS - 1 - probe_pixel.x;
        }
        else
        {
            copy_texel.x += abs(int(DDGI_DEPTH_TEXELS - 1 - probe_pixel.x) - 1);
            copy_texel.y += abs(int(DDGI_DEPTH_TEXELS - 1 - probe_pixel.y) - 1);
        }

        probes_depth_texture[threadID.xy] = probes_depth_texture[copy_texel];
    }
}

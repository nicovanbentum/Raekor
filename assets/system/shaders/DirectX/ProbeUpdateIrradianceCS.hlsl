#include "include/bindless.hlsli"
#include "include/packing.hlsli"
#include "include/common.hlsli"
#include "include/random.hlsli"
#include "include/ddgi.hlsli"

ROOT_CONSTANTS(ProbeUpdateRootConstants, rc)

groupshared float3 lds_ProbeRayDirections[DDGI_RAYS_PER_PROBE];
groupshared float3 lds_ProbeIrradianceRays[DDGI_RAYS_PER_PROBE];

[numthreads(DDGI_IRRADIANCE_TEXELS, DDGI_IRRADIANCE_TEXELS, 1)]
void main(uint3 threadID : SV_DispatchThreadID,  uint3 groupThreadID : SV_GroupThreadID, uint3 groupID : SV_GroupID, uint inGroupIndex : SV_GroupIndex) 
{
    Texture2D<float3> rays_irradiance_texture = ResourceDescriptorHeap[rc.mDDGIData.mRaysIrradianceTexture];
    RWTexture2D<float4> probes_irradiance_texture = ResourceDescriptorHeap[rc.mDDGIData.mProbesIrradianceTexture];
    
    // 1D index of the probe we are on, used to read the 192 ray hits from the ray tracing results
    uint probe_index = Index2DTo1D(groupID.xy, DDGI_PROBES_PER_ROW);
    
    // calculate how many rays the current thread should write to lds
    const uint rays_per_lane = DDGI_RAYS_PER_PROBE / (DDGI_IRRADIANCE_TEXELS * DDGI_IRRADIANCE_TEXELS);
    
    // unroll if possible as this number is usually quite small
    [unroll]
    for (uint i = 0; i < rays_per_lane; i++)
    {
        uint ray_index = inGroupIndex * rays_per_lane + i;
        lds_ProbeRayDirections[ray_index] = SphericalFibonnaci(ray_index, DDGI_RAYS_PER_PROBE);
        lds_ProbeIrradianceRays[ray_index] = rays_irradiance_texture[uint2(ray_index, probe_index)];
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    FrameConstants fc = gGetFrameConstants();
    
    // The 2D pixel coordinate on the probe's total texel area (with border)
    // every group is 1 probe, so get the 2d thread index within the group
    uint2 probe_pixel = groupThreadID.xy;
    
    bool is_border = probe_pixel.x == 0 || probe_pixel.x == (DDGI_IRRADIANCE_TEXELS - 1) ||
                     probe_pixel.y == 0 || probe_pixel.y == (DDGI_IRRADIANCE_TEXELS - 1);
    
    if (!is_border) 
    {
        float2 octahedral_uv = ((float2(probe_pixel) + 0.5) / DDGI_IRRADIANCE_TEXELS.xx) * 2.0 - 1.0;
        float3 octahedral_dir = OctDecode(octahedral_uv);
    
        float4 irradiance = 0.xxxx;
        
        for (uint ray_index = 0; ray_index < DDGI_RAYS_PER_PROBE; ray_index++) 
        {
            float3 ray_irradiance = lds_ProbeIrradianceRays[ray_index];
        
            float3 ray_dir = normalize(mul((float3x3) rc.mRandomRotationMatrix, lds_ProbeRayDirections[ray_index]));
            
            float weight = saturate(dot(octahedral_dir, ray_dir));
            
            if (weight > 0.0001)
            {
                irradiance += float4(ray_irradiance * weight, weight);
            }
        
        }
    
        if (irradiance.w > 0.0)
            irradiance.rgb /= irradiance.w;
        
        float3 prev_irradiance = probes_irradiance_texture[threadID.xy].rgb;
        
        float hysteresis = 0.985f;
        float3 avg_irradiance = lerp(irradiance.rgb, prev_irradiance, hysteresis);
        float3 final_irradiance = fc.mFrameCounter == 0 ? irradiance.rgb : avg_irradiance;

        probes_irradiance_texture[threadID.xy] = float4(final_irradiance, 1.0);

        // probes_irradiance_texture[threadID.xy] = float3(octahedral_uv * 0.5 + 0.5, 0.0);
    }
    
    AllMemoryBarrierWithGroupSync();
    
    if (is_border)
    {
        // Initialize the texel coordinate to copy to 0,0 in range [0 - NR_OF_TEXELS] (so basically the top left start of the probe texels)
        uint2 copy_texel = groupID.xy * DDGI_IRRADIANCE_TEXELS;
    
        // Trick is here is to wrap around by using the absolute of signed integers
        // e.g. border at 0,0 maps to abs(0,0 - 1,1) which is 1,1 inner pixel,
        // using the same for 6,6 maps to abs(6,6 - 1,1) which is 5,5
        if (probe_pixel.y > 0 && probe_pixel.y < DDGI_IRRADIANCE_TEXELS - 1)
        {
            copy_texel.x += abs(int(probe_pixel.x) - 1);
            copy_texel.y += DDGI_IRRADIANCE_TEXELS - 1 - probe_pixel.y;
        }
        else if (probe_pixel.x > 0 && probe_pixel.x < DDGI_IRRADIANCE_TEXELS - 1)
        {
            copy_texel.y += abs(int(probe_pixel.y) - 1);
            copy_texel.x += DDGI_IRRADIANCE_TEXELS - 1 - probe_pixel.x;
        }
        else 
        {
            copy_texel.x += abs(int(DDGI_IRRADIANCE_TEXELS - 1 - probe_pixel.x) - 1);
            copy_texel.y += abs(int(DDGI_IRRADIANCE_TEXELS - 1 - probe_pixel.y) - 1);
        }
    
        probes_irradiance_texture[threadID.xy] = probes_irradiance_texture[copy_texel];
    }
    
}
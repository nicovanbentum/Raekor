#include "include/bindless.hlsli"
#include "include/packing.hlsli"
#include "include/common.hlsli"
#include "include/random.hlsli"
#include "include/ddgi.hlsli"

ROOT_CONSTANTS(DDGIData, rc)

[numthreads(DDGI_IRRADIANCE_TEXELS, DDGI_IRRADIANCE_TEXELS, 1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    Texture2D<float3> rays_irradiance_texture = ResourceDescriptorHeap[rc.mRaysIrradianceTexture];
    RWTexture2D<float3> probes_irradiance_texture = ResourceDescriptorHeap[rc.mProbesIrradianceTexture];
    
    FrameConstants fc = gGetFrameConstants();
    
    // The 2D pixel coordinate on the probe's total texel area (with border)
    uint2 probe_pixel = threadID.xy % DDGI_IRRADIANCE_TEXELS.xx;
    
    // 2D index of the probe we are on
    uint2 probe_index_2d = uint2(threadID.xy / DDGI_IRRADIANCE_TEXELS);
    
    // 1D index of the probe we are on, used to read the 192 ray hits from the ray tracing results
    uint probe_index = Index2DTo1D(probe_index_2d, DDGI_PROBES_PER_ROW);
    
    bool is_border = probe_pixel.x == 0 || probe_pixel.x == (DDGI_IRRADIANCE_TEXELS - 1) ||
                     probe_pixel.y == 0 || probe_pixel.y == (DDGI_IRRADIANCE_TEXELS - 1);
    
    if (!is_border) {
        float2 octahedral_uv = (float2(probe_pixel) / DDGI_IRRADIANCE_TEXELS.xx) * 2.0 - 1.0;
        float3 octahedral_dir = OctDecode(octahedral_uv);
    
        float4 irradiance = 0.xxxx;
        
        for (uint ray_index = 0; ray_index < DDGI_RAYS_PER_PROBE; ray_index++) {
            float3 ray_irradiance = rays_irradiance_texture[uint2(ray_index, probe_index)];
        
            float3 trace_dir = SphericalFibonnaci(ray_index, DDGI_RAYS_PER_PROBE);
            float weight = max(dot(octahedral_dir, trace_dir), 0);
        
            irradiance += float4(ray_irradiance * weight, weight);
        }
    
        if (irradiance.w > 0.0)
            irradiance.rgb /= irradiance.w;
        
        float3 prev_irradiance = probes_irradiance_texture[threadID.xy];
        
        float hysteresis = 0.0;
        probes_irradiance_texture[threadID.xy] = lerp(irradiance.rgb, prev_irradiance, hysteresis);
        
        // probes_irradiance_texture[threadID.xy] = float3(octahedral_uv * 0.5 + 0.5, 0.0);
        
        return;
    }
    
    AllMemoryBarrierWithGroupSync();
    
    // Initialize the texel coordinateto copy to 0,0 in range [0 - NR_OF_TEXELS] (so basically the top left start of the probe texels)
    uint2 copy_texel = probe_index_2d * DDGI_IRRADIANCE_TEXELS;
    
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
    else {
        copy_texel.x += abs(int(DDGI_IRRADIANCE_TEXELS - 1 - probe_pixel.x) - 1);
        copy_texel.y += abs(int(DDGI_IRRADIANCE_TEXELS - 1 - probe_pixel.y) - 1);
    }
    
    probes_irradiance_texture[threadID.xy] = probes_irradiance_texture[copy_texel];
}
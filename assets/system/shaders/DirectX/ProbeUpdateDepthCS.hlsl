#include "include/bindless.hlsli"
#include "include/packing.hlsli"
#include "include/common.hlsli"
#include "include/random.hlsli"
#include "include/ddgi.hlsli"

ROOT_CONSTANTS(ProbeUpdateRootConstants, rc)

[numthreads(DDGI_DEPTH_TEXELS, DDGI_DEPTH_TEXELS, 1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    Texture2D<float> rays_depth_texture = ResourceDescriptorHeap[rc.mDDGIData.mRaysDepthTexture];
    RWTexture2D<float2> probes_depth_texture = ResourceDescriptorHeap[rc.mDDGIData.mProbesDepthTexture];
    
    // The 2D pixel coordinate on the probe's total texel area (with border)
    uint2 probe_pixel = threadID.xy % DDGI_DEPTH_TEXELS.xx;
    
    // 2D index of the probe we are on
    uint2 probe_index_2d = uint2(threadID.xy / DDGI_DEPTH_TEXELS);
    
    // 1D index of the probe we are on, used to read the 192 ray hits from the ray tracing results
    uint probe_index = Index2DTo1D(probe_index_2d, DDGI_PROBES_PER_ROW);
    
    bool is_border = probe_pixel.x == 0 || probe_pixel.x == (DDGI_DEPTH_TEXELS - 1) ||
                     probe_pixel.y == 0 || probe_pixel.y == (DDGI_DEPTH_TEXELS - 1);
    
    if (!is_border) {
        float2 octahedral_uv = (float2(probe_pixel) / DDGI_DEPTH_TEXELS.xx) * 2.0 - 1.0;
        float3 octahedral_dir = OctDecode(octahedral_uv);
    
        float3 depth = 0.xxx;
        
        for (uint ray_index = 0; ray_index < DDGI_RAYS_PER_PROBE; ray_index++) {
            float ray_depth = rays_depth_texture[uint2(ray_index, probe_index)];
            // limit the depth to the max distance between probes, if its further we would have picked a different probe anyway
            ray_depth = min(ray_depth, length(rc.mDDGIData.mProbeSpacing));
        
            float3 ray_dir = SphericalFibonnaci(ray_index, DDGI_RAYS_PER_PROBE);
            ray_dir = normalize(mul(rc.mRandomRotationMatrix, ray_dir));
            float weight = max(dot(octahedral_dir, ray_dir), 0);
        
            depth += float3(ray_depth * weight, ray_depth*ray_depth * weight, weight);
        }
    
        if (depth.z > 0.0)
            depth.rg /= depth.z;
        
        probes_depth_texture[threadID.xy] = depth.rg;
        return;
    }
    
    AllMemoryBarrierWithGroupSync();
    
    // Initialize the texel coordinateto copy to 0,0 in range [0 - nr_of_texels] (so basically the top left start of the probe texels)
    uint2 copy_texel = probe_index_2d * DDGI_DEPTH_TEXELS;
    
    // Trick is here is to wrap around by using the absolute of signed integers
    // e.g. border at 0,0 maps to abs(0,0 - 1,1) which is 1,1 inner pixel,
    // using the same for 6,6 maps to abs(6,6 - 1,1) which is 5,5
    if (probe_pixel.y > 0 && probe_pixel.y < DDGI_DEPTH_TEXELS - 1) {
        copy_texel.x += abs(int(probe_pixel.x) - 1);
        copy_texel.y += DDGI_DEPTH_TEXELS - 1 - probe_pixel.y;
    }
    else if (probe_pixel.x > 0 && probe_pixel.x < DDGI_DEPTH_TEXELS - 1)
    {
        copy_texel.y += abs(int(probe_pixel.y) - 1);
        copy_texel.x += DDGI_DEPTH_TEXELS - 1 - probe_pixel.x;
    }
    else {
        copy_texel.x += abs(int(DDGI_DEPTH_TEXELS - 1 - probe_pixel.x) - 1);
        copy_texel.y += abs(int(DDGI_DEPTH_TEXELS - 1 - probe_pixel.y) - 1);
    }
    
    probes_depth_texture[threadID.xy] = probes_depth_texture[copy_texel];
}
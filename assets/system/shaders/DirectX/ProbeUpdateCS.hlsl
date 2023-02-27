#include "include/bindless.hlsli"
#include "include/packing.hlsli"
#include "include/common.hlsli"
#include "include/random.hlsli"
#include "include/ddgi.hlsli"

ROOT_CONSTANTS(ProbeUpdateRootConstants, rc)



[numthreads(1, 1, 1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    /* EXAMPLE DDGI PROBE TEXTURE ARRAY
            8 * 9
        ------------- 
        |           | 
        |           |
        | 9 layers  | 8 * 9 
        |           | 
        |           | 
        -------------  

        threadID.x = horizontal pixel pos, threadID.y = vertical pixel pos, threadID.z = texture array layer
    */
    Texture2D<float3> rays_irradiance_texture = ResourceDescriptorHeap[rc.mRaysIrradianceTexture];
    RWTexture2DArray<float3> probes_irradiance_texture = ResourceDescriptorHeap[rc.mProbesIrradianceTexture];
    
    FrameConstants fc = gGetFrameConstants();
    
    const float2 probe_uv = float2(threadID.xy % DDGI_PROBE_IRRADIANCE_RESOLUTION.xx) / DDGI_PROBE_IRRADIANCE_RESOLUTION.xx;
    float3 octa_dir = OctDecode(probe_uv);
    
    uint2 probe_pos = threadID.xy / DDGI_PROBE_IRRADIANCE_RESOLUTION;
    uint3 probe_coord = uint3(probe_pos.x, threadID.z, probe_pos.y);
    uint probe_index = GetDDGIProbeIndex(probe_coord, uint3(9, 9, 9)); // TODO: dont hardcode 9 lol
    
    float4 irradiance = 0.xxxx;
    
    for (uint ray_index = 0; ray_index < DDGI_RAYS_PER_PROBE; ray_index++) {
        uint2 rays_texture_index = uint2(ray_index, probe_index);
        float3 ray_irradiance = rays_irradiance_texture[rays_texture_index];
        
        float3 trace_dir = SphericalFibonnaci(ray_index, DDGI_RAYS_PER_PROBE);
        float weight = max(dot(octa_dir, trace_dir), 0);
        
        irradiance += float4(ray_irradiance * weight, weight);
    }
    
    irradiance.xyz /= irradiance.w;
    irradiance.w = 1.0;
    
    probes_irradiance_texture[threadID.xyz] = irradiance.xyz;
}